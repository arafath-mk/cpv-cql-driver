#pragma once
#include <vector>
#include <core/shared_ptr.hh>
#include "CqlNodeConfiguration.hpp"

namespace cql {
	/** Interface use to manage node configurations and choose node based on some strategy */
	class CqlNodeCollection {
	public:
		/** Choose a node for the new database connection */
		virtual seastar::shared_ptr<CqlNodeConfiguration> chooseOneNode() = 0;
		
		/** Report connect to this node has failed */
		virtual void reportFailure(const seastar::shared_ptr<CqlNodeConfiguration>& node) = 0;

		/** Report connect to this node has been successful */
		virtual void reportSuccess(const seastar::shared_ptr<CqlNodeConfiguration>& node) = 0;

		/** Virtual destructor */
		virtual ~CqlNodeCollection() = default;

		/** Create a default implementation of CqlNodeCollection */
		static seastar::shared_ptr<CqlNodeCollection> create(
			const std::vector<CqlNodeConfiguration>& initialNodes);
	};
}
