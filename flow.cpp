#include <ctime>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <list>
#include <queue>
#include <random>
#include <thread>
#include <unordered_map>
#include <vector>

class Network
{
public:
	struct Edge;
	enum class Node;

	const Node* addNode(Node node) noexcept;

	void connect(const Node* from, const Node* to, double capacity) noexcept;

	/**
	 * @brief Solve the max flow problem and return the maximum source to sink
	 * flow.
	 *
	 * Modifies the graph in the process.
	 */
	double solve() noexcept;

	std::size_t nodes() const noexcept { return nodes_.size(); }

	std::size_t edges() const noexcept { return edges_.size(); }

private:
	std::list<Node> nodes_;
	std::list<Edge> edges_;
	std::unordered_map<const Node*, std::vector<Edge*>> inbound_;
	std::unordered_map<const Node*, std::vector<Edge*>> outbound_;
};

struct Network::Edge
{
	double capacity;
	double flow;
	Node* from;
	Node* to;
};

enum class Network::Node
{
	Source,
	Sink,
	Junction
};

inline const Network::Node* Network::addNode(Node node) noexcept
{
	nodes_.push_back(std::move(node));
	return &nodes_.back();
}

inline void Network::connect(const Node* from,
                             const Node* to,
                             double capacity) noexcept
{
	edges_.push_back(
	    {capacity, 0.0, const_cast<Node*>(from), const_cast<Node*>(to)});
	inbound_[to].push_back(&edges_.back());
	outbound_[from].push_back(&edges_.back());
}

inline double Network::solve() noexcept
{
	// Simplify to standard max flow problem.

	std::vector<Node*> sources;
	std::vector<Node*> sinks;

	for (auto& node : nodes_)
	{
		if (node == Node::Sink)
		{
			sinks.push_back(&node);
		}
		else if (node == Node::Source)
		{
			sources.push_back(&node);
		}
	}

	Node* source = sources.front();
	if (!sources.empty())
	{
		// Make super source.

		source = const_cast<Node*>(addNode(Node::Source));

		for (auto& s : sources)
		{
			connect(source, s, std::numeric_limits<double>::max());
			*s = Node::Junction;
		}
	}

	Node* sink = sinks.front();
	if (!sinks.empty())
	{
		// Make super sink.

		sink = const_cast<Node*>(addNode(Node::Sink));

		for (auto& s : sinks)
		{
			connect(s, sink, std::numeric_limits<double>::max());
			*s = Node::Junction;
		}
	}

	// Apply Edmonds-Karp algorithm:

	double flow = 0.0;

	while (true)
	{
		std::unordered_map<Node*, Edge*> path;

		std::queue<Node*> queue;
		queue.push(source);

		// Find shortest path from source to sink.
		while (!queue.empty() && !path.count(sink))
		{
			Node* next = queue.front();
			queue.pop();

			for (Edge* e : outbound_[next])
			{
				Node* node = e->to;
				if (e->capacity - e->flow >
				        std::numeric_limits<double>::epsilon() &&
				    !path.count(node))
				{
					path[node] = e;
					queue.push(node);
				}
			}

			for (Edge* e : inbound_[next])
			{
				Node* node = e->from;
				if (e->flow > std::numeric_limits<double>::epsilon() &&
				    !path.count(node))
				{
					path[node] = e;
					queue.push(node);
				}
			}
		}

		if (!path.count(sink))
		{
			break;
		}

		double pathFlow = std::numeric_limits<double>::max();

		// Find path capacity.
		for (Node* node = sink; node != source;)
		{
			Edge* e  = path[node];
			pathFlow = std::min(
			    pathFlow, e->to == node ? e->capacity - e->flow : e->flow);
			node = e->to == node ? e->from : e->to;
		}

		// Update path flow.
		for (Node* node = sink; node != source;)
		{
			Edge* e = path[node];
			e->to == node ? e->flow += pathFlow : e->flow -= pathFlow;
			node = e->to == node ? e->from : e->to;
		}

		// Update global flow.
		flow += pathFlow;
	}

	return flow;
}

Network build_random_network(
    int sources, int sinks, int junctions, int edges, double maxCapacity)
{
	Network network;
	std::vector<const Network::Node*> nodes;

	for (int i = 0; i < sources; ++i)
		nodes.push_back(network.addNode(Network::Node::Source));

	for (int i = 0; i < sinks; ++i)
		nodes.push_back(network.addNode(Network::Node::Sink));

	for (int i = 0; i < junctions; ++i)
		nodes.push_back(network.addNode(Network::Node::Junction));

	std::mt19937 random(std::time(NULL));
	std::uniform_int_distribution<int> ints(0, nodes.size() - 1);
	std::uniform_real_distribution<double> doubles(1.0, maxCapacity);

	for (int i = 0; i < edges; ++i)
	{
		const Network::Node* from = nullptr;
		const Network::Node* to   = nullptr;

		while (from == to)
		{
			from = nodes[ints(random)];
			to   = nodes[ints(random)];
		}

		network.connect(from, to, doubles(random));
	}

	return network;
}

Network get_random_network(int maxNodes, int maxEdges)
{
	std::random_device rd;
	std::mt19937 random(rd());
	std::uniform_int_distribution<int> sources(1, maxNodes / 10);
	std::uniform_int_distribution<int> sinks(1, maxNodes / 10);
	std::uniform_int_distribution<int> junctions(1, maxNodes / 10 * 8);
	std::uniform_int_distribution<int> edges(1, maxEdges);
	std::uniform_real_distribution<double> capacities(0.1, 1000.0);

	return build_random_network(sources(random),
	                            sinks(random),
	                            junctions(random),
	                            edges(random),
	                            capacities(random));
}

int main(int argc, char** argv)
{
	struct Result
	{
		std::size_t nodes;
		std::size_t edges;
		double flow;
		double time;
	};

	if (argc != 5)
	{
		std::cerr << "usage: " << argv[0]
		          << " <maxNodes> <maxEdges> <runs> <output>" << std::endl;
		return EXIT_FAILURE;
	}

	std::ofstream output(argv[4]);
	if (!output.is_open())
	{
		std::cerr << "failed to open output file: " << argv[3] << std::endl;
		return EXIT_FAILURE;
	}

	output << "nodes,edges,flow,time\n";

	int maxNodes = std::atoi(argv[1]);
	int maxEdges = std::atoi(argv[2]);
	int runs     = std::atoi(argv[3]);

	auto count = std::thread::hardware_concurrency();
	std::vector<std::future<Result>> futures;

	int completed = 0;
	for (int i = 0; i < runs; ++i)
	{
		while (futures.size() >= count)
		{
			for (auto it = futures.begin(); it != futures.end();)
			{
				if (it->wait_for(std::chrono::milliseconds(1)) ==
				    std::future_status::ready)
				{
					Result r = it->get();
					output << r.nodes << ',' << r.edges << ',' << r.flow << ','
					       << r.time << '\n';
					it = futures.erase(it);
					std::cout << "\r[" << ++completed << '/' << runs << "]"
					          << std::flush;
				}
				else
				{
					++it;
				}
			}
		}

		futures.push_back(
		    std::async(std::launch::async, [maxNodes, maxEdges]() -> Result {
			    Network network = get_random_network(maxNodes, maxEdges);

			    std::clock_t start = std::clock();
			    double flow        = network.solve();
			    std::clock_t end   = std::clock();

			    return Result{network.nodes(),
			                  network.edges(),
			                  flow,
			                  ((double)(end - start)) / CLOCKS_PER_SEC};
		    }));
	}

	for (auto& f : futures)
	{
		Result r = f.get();
		output << r.nodes << ',' << r.edges << ',' << r.flow << ',' << r.time
		       << '\n';
		std::cout << "\r[" << ++completed << '/' << runs << "]" << std::flush;
	}
}
