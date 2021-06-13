#include "flow.h"

#include <queue>
#include <variant>
#include <functional>
#include "log.h"

using namespace std;

using Node = FlowGraphNode;

Node pseudoNode(Edge const& _edge)
{
	return make_tuple(_edge.from, _edge.token);
}

/// Concatenate the contents of a container onto a vector, move variant.
template <class T, class U> vector<T>& operator+=(vector<T>& _a, U&& _b)
{
	std::move(_b.begin(), _b.end(), std::back_inserter(_a));
	return _a;
}

template <class K, class V, class F>
void erase_if(map<K, V>& _container, F const& _fun)
{
    log_debug("-> erase_if(_container: %li, _fun: F)", _container.size());

	for (auto it = _container.begin(); it != _container.end();)
		 if (_fun(*it))
			it = _container.erase(it);
		 else
			++it;

    log_debug("<- erase_if(_container: %li, _fun: F)", _container.size());
}

static map<Node, map<Node, Int>> _adjacencies;
static bool _adjacenciesDirty = true;

/// Turns the edge set into an adjacency list.
/// At the same time, it generates new pseudo-nodes to cope with the multi-edges.
map<Node, map<Node, Int>> computeAdjacencies(set<Edge> const& _edges)
{
    log_debug("-> computeAdjacencies(_edges: %li)", _edges.size());

    if (!_adjacenciesDirty) {
        log_debug("<- computeAdjacencies(_edges: %li) - cached", _edges.size());
        return _adjacencies;
    }

    _adjacencies = map<Node, map<Node, Int>>();

	for (Edge const& edge: _edges)
	{
        auto pseudo = pseudoNode(edge);
		// One edge from "from" to "from x token" with a capacity as the max over
		// all contributing edges (the balance of the sender)
        _adjacencies[edge.from][pseudo] = max(edge.capacity, _adjacencies[edge.from][pseudo]);
		// Another edge from "from x token" to "to" with its own capacity (based on the trust)
        _adjacencies[pseudo][edge.to] = edge.capacity;
	}

	log_debug("<- computeAdjacencies(_edges: %li)", _edges.size());
    _adjacenciesDirty = false;
	return _adjacencies;
}

vector<pair<Node, Int>> sortedByCapacity(map<Node, Int> const& _capacities)
{
    log_debug("-> sortedByCapacity(_capacities: %li)", _capacities.size());
	vector<pair<Node, Int>> r(_capacities.begin(), _capacities.end());
	sort(r.begin(), r.end(), [](pair<Node, Int> const& _a, pair<Node, Int> const& _b) {
		return make_pair(get<1>(_a), get<0>(_a)) > make_pair(get<1>(_b), get<0>(_b));
	});
    log_debug("<- sortedByCapacity(_capacities: %li)", _capacities.size());
	return r;
}

pair<Int, map<Node, Node>> augmentingPath(
	Address const& _source,
	Address const& _sink,
	map<Node, map<Node, Int>> const& _capacity
)
{
	if (_source == _sink || !_capacity.count(_source))
		return {Int(0), {}};

	map<Node, Node> parent;
	queue<pair<Node, Int>> q;
	q.emplace(_source, Int::max());

	while (!q.empty())
	{
		//cout << "Queue size: " << q.size() << endl;
		//cout << "Parent relation size: " << parent.size() << endl;
		auto [node, flow] = q.front();
		q.pop();
		if (!_capacity.count(node))
			continue;
		for (auto const& [target, capacity]: sortedByCapacity(_capacity.at(node)))
			if (!parent.count(target) && Int(0) < capacity)
			{
				parent[target] = node;
				Int newFlow = min(flow, capacity);
				if (target == Node{_sink})
					return make_pair(move(newFlow), move(parent));
				q.emplace(target, move(newFlow));
			}
	}
	return {Int(0), {}};
}

/// Extract the next list of transfers until we get to a situation where
/// we cannot transfer the full balance and start over.
vector<Edge> extractNextTransfers(map<Node, map<Node, Int>>& _usedEdges, map<Address, Int>& _nodeBalances)
{
    auto initialEdgesSize = _usedEdges.size();
    auto initialNodesSize = _nodeBalances.size();
    log_debug("-> extractNextTransfers(_usedEdges: %li, _nodeBalances: %li)", initialEdgesSize, initialNodesSize);
	vector<Edge> transfers;

	for (auto& [node, balance]: _nodeBalances)
		for (auto& edge: _usedEdges[node])
		{
			Node const& intermediate = edge.first;
			for (auto& [toNode, capacity]: _usedEdges[intermediate])
			{
				auto const& [from, token] = std::get<tuple<Address, Address>>(intermediate);
				Address to = std::get<Address>(toNode);
				if (capacity == Int(0))
					continue;
				if (balance < capacity)
				{
					// We do not have enough balance yet, there will be another transfer along this edge.
					if (!transfers.empty())
						return transfers;
					else
						continue;
				}
				transfers.push_back(Edge{from, to, token, capacity});
				balance -= capacity;
				_nodeBalances[to] += capacity;
				capacity = Int(0);
			}
		}

    log_debug("   extractNextTransfers(_usedEdges: %li, _nodeBalances: %li): '_usedEdges' size: %li, '_nodeBalances' size: %li", initialEdgesSize, initialNodesSize, _usedEdges.size(), _nodeBalances.size());
	erase_if(_nodeBalances, [](auto& _a) { return _a.second == Int(0); });
    log_debug("   extractNextTransfers(_usedEdges: %li, _nodeBalances: %li): '_usedEdges' size: %li, '_nodeBalances' size: %li", initialEdgesSize, initialNodesSize, _usedEdges.size(), _nodeBalances.size());
    log_debug("<- extractNextTransfers(_usedEdges: %li, _nodeBalances: %li)", initialEdgesSize, initialNodesSize);
	return transfers;
}


vector<Edge> extractTransfers(Address const& _source, Address const& _sink, Int _amount, map<Node, map<Node, Int>> _usedEdges)
{
    auto initialEdgesSize = _usedEdges.size();
    log_debug("-> extractTransfers(_source: '%s', _sink: '%s', _amount: %s, _usedEdges: %li, _nodeBalances: %li)",
              to_string(_source).c_str(),
              to_string(_sink).c_str(),
              to_string(_amount).c_str(),
              initialEdgesSize);

	vector<Edge> transfers;

	map<Address, Int> nodeBalances;
	nodeBalances[_source] = _amount;
	while (
		!nodeBalances.empty() &&
		(nodeBalances.size() > 1 || nodeBalances.begin()->first != _sink)
	) {
        transfers += extractNextTransfers(_usedEdges, nodeBalances);
    }

    log_debug("<- extractTransfers(_source: '%s', _sink: '%s', _amount: %s, _usedEdges: %li, _nodeBalances: %li)",
              to_string(_source).c_str(),
              to_string(_sink).c_str(),
              to_string(_amount).c_str(),
              initialEdgesSize);

    return transfers;
}

pair<Int, vector<Edge>> computeFlow(
	Address const& _source,
	Address const& _sink,
	set<Edge> const& _edges,
	Int _requestedFlow
)
{
    log_debug("-> computeFlow(_source: '%s', _sink: '%s', _edges: %li, _requestedFlow: %s)",
              to_string(_source).c_str(),
              to_string(_sink).c_str(),
              _edges.size(),
              to_string(_requestedFlow).c_str());

	map<Node, map<Node, Int>> adjacencies = computeAdjacencies(_edges);
	map<Node, map<Node, Int>> capacities = adjacencies;

    log_debug("   computeFlow(_source: '%s', _sink: '%s', _edges: %li, _requestedFlow: %s): %li nodes (including pseudo-nodes) and %li adjacencies from %li edges",
              to_string(_source).c_str(),
              to_string(_sink).c_str(),
              _edges.size(),
              to_string(_requestedFlow).c_str(),
              capacities.size(),
              adjacencies.size(),
              _edges.size());

	map<Node, map<Node, Int>> usedEdges;

	Int flow{0};
	while (flow < _requestedFlow)
	{
		auto [newFlow, parents] = augmentingPath(_source, _sink, capacities);
		//cout << "Found augmenting path with flow " << newFlow << endl;
		if (newFlow == Int(0))
			break;
		if (flow + newFlow > _requestedFlow)
			newFlow = _requestedFlow - flow;
		flow += newFlow;
		for (Node node = _sink; node != Node{_source}; )
		{
			Node const& prev = parents[node];
			capacities[prev][node] -= newFlow;
			capacities[node][prev] += newFlow;
			// TODO still not sure about this one.
			if (!adjacencies.count(node) || !adjacencies.at(node).count(prev) || adjacencies.at(node).at(prev) == Int(0))
				// real edge
				usedEdges[prev][node] += newFlow;
			else
				// (partial) edge removal
				usedEdges[node][prev] -= newFlow;
			node = prev;
		}
	}

    log_debug("<- computeFlow(_source: '%s', _sink: '%s', _edges: %li, _requestedFlow: %s)",
              to_string(_source).c_str(),
              to_string(_sink).c_str(),
              _edges.size(),
              to_string(_requestedFlow).c_str());

	return {flow, extractTransfers(_source, _sink, flow, usedEdges)};
}
