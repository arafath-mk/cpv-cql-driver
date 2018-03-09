#include <limits>
#include <net/dns.hh>
#include <core/reactor.hh>
#include <CQLDriver/Common/Exceptions/NotImplementedException.hpp>
#include <CQLDriver/Common/Exceptions/NetworkException.hpp>
#include <CQLDriver/Common/Exceptions/LogicException.hpp>
#include <CQLDriver/Common/Exceptions/ConnectionInitializeException.hpp>
#include "./Connectors/ConnectorFactory.hpp"
#include "./Authenticators/AuthenticatorFactory.hpp"
#include "./RequestMessages/RequestMessageFactory.hpp"
#include "./RequestMessages/OptionsMessage.hpp"
#include "./RequestMessages/StartupMessage.hpp"
#include "./ResponseMessages/ResponseMessageFactory.hpp"
#include "./Connection.hpp"

namespace cql {
	/** Initialize connection and wait until it's ready to send ordinary messages */
	seastar::future<> Connection::ready() {
		if (isReady_) {
			return seastar::make_ready_future<>();
		}
		auto self = shared_from_this();
		return seastar::futurize_apply([self] {
			// connect to database server
			seastar::socket_address address;
			if (self->nodeConfiguration_->getIpAddress(
				address, self->sessionConfiguration_->getDnsCacheTime())) {
				return self->connector_->connect(*self->nodeConfiguration_, address);
			} else {
				return seastar::net::dns::resolve_name(self->nodeConfiguration_->getAddress().first)
					.then([self](seastar::net::inet_address inetAddress) {
					if (inetAddress.in_family() == seastar::net::inet_address::family::INET) {
						seastar::socket_address address(seastar::ipv4_addr(
							inetAddress, self->nodeConfiguration_->getAddress().second));
						self->nodeConfiguration_->updateIpAddress(address);
						return self->connector_->connect(*self->nodeConfiguration_, address);
					} else {
						// seastar's socket_address only support ipv4 for now
						return seastar::make_exception_future<seastar::connected_socket>(
							NotImplementedException(CQL_CODEINFO, "ipv6 address is unsupported yet"));
					}
				});
			}
		}).then([self] (seastar::connected_socket fd) {
			// connect successful
			self->socket_ = SocketHolder(std::move(fd));
		}).handle_exception([self] (std::exception_ptr ex) {
			// connect failed
			return seastar::make_exception_future(NetworkException(
				CQL_CODEINFO, "connect to",
				self->nodeConfiguration_->getAddress().first, "failed:\n", ex));
		}).then([self] {
			// send OPTION
			auto optionMessage = RequestMessageFactory::makeRequestMessage<OptionsMessage>();
			return self->sendMessage(std::move(optionMessage), self->streamZero_);
		}).then([self] {
			// receive SUPPORTED
			return self->waitNextMessage(self->streamZero_);
		}).then([self] (auto message) {
			// send STARTUP
			if (message->getHeader().getOpCode() != MessageType::Supported) {
				return seastar::make_exception_future(LogicException(
					CQL_CODEINFO, "unexcepted response to OPTION message:", message->toString()));
			}
			auto startupMessage = RequestMessageFactory::makeRequestMessage<StartupMessage>();
			return self->sendMessage(std::move(startupMessage), self->streamZero_);
		}).then([self] {
			// perform authentication
			return self->authenticator_->authenticate(self, self->streamZero_);
		}).then([self] {
			// the connection is ready
			self->isReady_ = true;
		}).handle_exception([self] (std::exception_ptr ex) {
			// initialize failed
			return seastar::make_exception_future(ConnectionInitializeException(
				CQL_CODEINFO, "initialize connection to",
				self->nodeConfiguration_->getAddress().first, "failed:\n", ex));
		});
	}

	/** Open a new stream */
	ConnectionStream Connection::openStream() {
		if (freeStreamIds_->empty()) {
			// no available stream
			return ConnectionStream();
		} else {
			auto streamId = freeStreamIds_->back();
			freeStreamIds_->pop_back();
			return ConnectionStream(streamId, freeStreamIds_);
		}
	}

	/** Get how many free streams available */
	std::size_t Connection::getFreeStreamsCount() const {
		return freeStreamIds_->size();
	}

	/** Send a message to the given stream and wait for success */
	seastar::future<> Connection::sendMessage(
		Object<RequestMessageBase>&& message,
		const ConnectionStream& stream) {
		// ensure only one message is sending at the same time
		message->getHeader().setStreamId(stream.getStreamId());
		auto previousSendingFuture = std::move(sendingFuture_);
		seastar::promise<> sendingPromise;
		sendingFuture_ = sendingPromise.get_future();
		auto self = shared_from_this();
		return seastar::do_with(
			std::move(self),
			std::move(message),
			std::move(sendingPromise),
			std::move(previousSendingFuture), [] (
			auto& self,
			auto& message,
			auto& sendingPromise,
			auto& previousSendingFuture) {
			return previousSendingFuture.then([&self, &message] {
				// encode message to binary data
				self->sendingBuffer_.resize(0);
				message->getHeader().encodeHeaderPre(self->connectionInfo_, self->sendingBuffer_);
				message->encodeBody(self->connectionInfo_, self->sendingBuffer_);
				message->getHeader().encodeHeaderPost(self->connectionInfo_, self->sendingBuffer_);
				// send the encoded binary data
				return self->socket_.out().write(self->sendingBuffer_).then([&self] {
					return self->socket_.out().flush();
				});
			}).then([&self, &sendingPromise] {
				// this message is successful, allow next message to start sending
				sendingPromise.set_value();
			}).handle_exception([&self, &sendingPromise] (std::exception_ptr ex) {
				// this message is failed, report the error to both waiters
				sendingPromise.set_exception(ex);
				return seastar::make_exception_future(NetworkException(
					CQL_CODEINFO, "send message to",
					self->nodeConfiguration_->getAddress().first, "failed:", ex));
			});
		});
	}

	/** Wait for the next message from the given stream */
	seastar::future<Object<ResponseMessageBase>> Connection::waitNextMessage(
		const ConnectionStream& stream) {
		// try getting message directly from received queue
		auto streamId = stream.getStreamId();
		auto& queue = receivedMessageQueueMap_.at(streamId);
		if (!queue.empty()) {
			return seastar::make_ready_future<Object<ResponseMessageBase>>(queue.pop());
		}
		// message not available, try receiving it from network
		auto& promiseSlot = receivingPromiseMap_.at(streamId);
		if (promiseSlot.first) {
			return seastar::make_exception_future<Object<ResponseMessageBase>>(
				LogicException(CQL_CODEINFO, "there already a message waiter registered for this stream"));
		}
		promiseSlot.first = true;
		promiseSlot.second = {};
		if (receivingPromiseCount_++ == 0) {
			// the first who made promise has responsibility to start the receiver
			auto self = shared_from_this();
			seastar::do_with(std::move(self), [](auto& self) {
				return seastar::repeat([&self] {
					// receive message header
					return self->socket_.in().read_exactly(self->connectionInfo_.getHeaderSize())
					.then([&self] (seastar::temporary_buffer<char>&& buf) {
						MessageHeader header;
						header.decodeHeader(self->connectionInfo_, std::move(buf));
						auto bodyLength = header.getBodyLength();
						if (bodyLength > self->connectionInfo_.getMaximumMessageBodySize()) {
							self->close(joinString(" ", "message body too large:", bodyLength));
							return seastar::make_ready_future<seastar::stop_iteration>(
								seastar::stop_iteration::yes);
						}
						// receive message body
						return self->socket_.in().read_exactly(bodyLength)
						.then([&self, header=std::move(header)] (
							seastar::temporary_buffer<char>&& buf) mutable {
							auto message = ResponseMessageFactory::makeResponseMessage(std::move(header));
							message->decodeBody(self->connectionInfo_, std::move(buf));
							// find the corresponding promise
							auto streamId = message->getHeader().getStreamId();
							if (streamId >= self->receivedMessageQueueMap_.size()) {
								self->close(joinString(" ",
									"stream id of received message out of range:", streamId));
								return seastar::stop_iteration::yes;
							}
							auto& promiseSlot = self->receivingPromiseMap_[streamId];
							if (promiseSlot.first) {
								// pass message directly to the promise
								promiseSlot.first = false;
								promiseSlot.second.set_value(std::move(message));
								if (self->receivingPromiseCount_ == 0) {
									self->close("incorrect receiving promise count, should be logic error");
									return seastar::stop_iteration::yes;
								}
								--self->receivingPromiseCount_;
							} else {
								// enqueue message to received queue
								if (!self->receivedMessageQueueMap_[streamId].push(std::move(message))) {
									self->close(joinString(" ",
										"max pending messages is reached, stream id:", streamId));
									return seastar::stop_iteration::yes;
								}
							}
							if (self->receivingPromiseCount_ == 0) {
								// no more message waiter is waiting, exit receiver
								return seastar::stop_iteration::yes;
							} else {
								// continue to receive next message
								return seastar::stop_iteration::no;
							}
						});
					});
				}).handle_exception([&self] (std::exception_ptr ex) {
					self->close(joinString(" ", "receive message failed:", ex));
				});
			});
		}
		return promiseSlot.second.get_future();
	}

	/** Constructor */
	Connection::Connection(
		const seastar::lw_shared_ptr<SessionConfiguration>& sessionConfiguration,
		const seastar::lw_shared_ptr<NodeConfiguration>& nodeConfiguration) :
		Connection(
			sessionConfiguration,
			nodeConfiguration,
			ConnectorFactory::getConnector(*nodeConfiguration),
			AuthenticatorFactory::getAuthenticator(*nodeConfiguration)) { }

	/** Constructor */
	Connection::Connection(
		const seastar::lw_shared_ptr<SessionConfiguration>& sessionConfiguration,
		const seastar::lw_shared_ptr<NodeConfiguration>& nodeConfiguration,
		const seastar::shared_ptr<ConnectorBase>& connector,
		const seastar::shared_ptr<AuthenticatorBase>& authenticator) :
		sessionConfiguration_(sessionConfiguration),
		nodeConfiguration_(nodeConfiguration),
		connector_(connector),
		authenticator_(authenticator),
		socket_(),
		isReady_(false),
		connectionInfo_(),
		freeStreamIds_(seastar::make_lw_shared<decltype(freeStreamIds_)::element_type>()),
		streamZero_(0, freeStreamIds_),
		sendingFuture_(seastar::make_ready_future<>()),
		sendingBuffer_(),
		receivingPromiseMap_(nodeConfiguration_->getMaxStreams()),
		receivedMessageQueueMap_(),
		receivingPromiseCount_(0) {
		// initialize free stream ids, exclude stream zero (which is for internal communication)
		freeStreamIds_->resize(nodeConfiguration_->getMaxStreams() - 1);
		for (std::size_t i = 0, j = freeStreamIds_->size(); i < j; ++i) {
			(*freeStreamIds_)[i] = j-i;
		}
		// initialize received message queues
		for (std::size_t i = 0, j = nodeConfiguration_->getMaxStreams(); i < j; ++i) {
			receivedMessageQueueMap_.emplace_back(nodeConfiguration->getMaxPendingMessages());
		}
	}

	/** Destructor */
	Connection::~Connection() {
		try {
			close("close from destructor");
		} catch (...) {
			std::cerr << std::current_exception() << std::endl;
		}
	}

	/** Close the connection */
	void Connection::close(const seastar::sstring& errorMessage) {
		// close the socket and reset the ready state
		socket_ = SocketHolder();
		isReady_ = false;
		// notify all message waiters
		receivingPromiseCount_ = 0;
		for (auto& slot : receivingPromiseMap_) {
			if (slot.first) {
				slot.first = false;
				slot.second.set_exception(NetworkException(CQL_CODEINFO, errorMessage));
			}
		}
	}
}
