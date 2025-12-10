#include <ctime>
#include <fstream>
#include <iostream>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>

class Graph
{
public:
	struct Node;

	const Node* insert(const std::string& name) noexcept;
	void connect(const Node* from, const Node* to) noexcept;
	void remove(const Node* node) noexcept;
	const Node* highestNetDegree() noexcept;

	bool empty() const noexcept { return players_.empty(); }

private:
	struct Degree;

	std::list<Node> players_;
	std::unordered_map<const Node*, Degree> degree_;
	std::unordered_map<const Node*, std::unordered_set<const Node*>> edges_;

	/// Because this is a directed graph, we need another map to store nodes
	/// that are being pointed to in order to handle deletion.
	std::unordered_map<const Node*, std::unordered_set<const Node*>>
	    associated_;
};

struct Graph::Node
{
	std::string name;
};

struct Graph::Degree
{
	unsigned int in  = 0;
	unsigned int out = 0;
};

inline const Graph::Node* Graph::insert(const std::string& name) noexcept
{
	players_.push_back({name});
	return &players_.back();
}

inline void Graph::connect(const Node* from, const Node* to) noexcept
{
	edges_[from].insert(to);
	associated_[to].insert(from);
	++degree_[to].in;
	++degree_[from].out;
}

inline void Graph::remove(const Node* node) noexcept
{
	for (const auto& n : edges_[node])
	{
		--degree_[n].in;
	}

	for (const auto& n : associated_[node])
	{
		auto result = edges_.find(n);
		if (result != edges_.end() && result->second.count(node) != 0)
		{
			result->second.erase(node);
			--degree_[n].out;
		}
	}

	for (auto it = players_.begin(); it != players_.end(); ++it)
	{
		if (&*it == node)
		{
			players_.erase(it);
			break;
		}
	}

	edges_.erase(node);
	associated_.erase(node);
	degree_.erase(node);
}

inline const Graph::Node* Graph::highestNetDegree() noexcept
{
	const Node* best = nullptr;
	int net;

	for (const auto& [node, degree] : degree_)
	{
		int n = degree.in - degree.out;
		if (best == nullptr || net < n)
		{
			best = node;
			net  = n;
		}
	}

	return best;
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		std::cerr << "usage: " << argv[0] << " <pgn-file>" << std::endl;
		return EXIT_FAILURE;
	}

	std::ifstream pgn(argv[1]);
	if (!pgn.is_open())
	{
		std::cerr << "failed to open: " << argv[1] << std::endl;
		return EXIT_FAILURE;
	}

	Graph graph;
	std::unordered_map<std::string, const Graph::Node*> nodes;

	std::string white;
	std::string black;

	enum
	{
		White,
		Black,
		Draw
	} result;

	bool parsing = false;

	for (std::string line; std::getline(pgn, line);)
	{
		if (line[0] == '[')
		{
			parsing = true;

			// Parsing pgn files [Key "Value"] pairs:
			std::string key = line.substr(1, line.find_first_of(' ') - 1);
			auto first      = line.find_first_of('"') + 1;
			std::string value =
			    line.substr(first, line.find_last_of('"') - first);

			if (key == "White")
			{
				white = value;
			}
			else if (key == "Black")
			{
				black = value;
			}
			else if (key == "Result")
			{
				if (value[0] == '0')
				{
					result = Black;
				}
				else if (value[1] == '-')
				{
					result = White;
				}
				else
				{
					result = Draw;
				}
			}
		}
		else if (parsing)
		{
			parsing = false;

			if (nodes.count(white) == 0)
			{
				nodes.emplace(white, graph.insert(white));
			}

			if (nodes.count(black) == 0)
			{
				nodes.emplace(black, graph.insert(black));
			}

			switch (result)
			{
				case White: graph.connect(nodes[black], nodes[white]); break;
				case Black: graph.connect(nodes[white], nodes[black]); break;
				case Draw:
					// Draws result in an undirected edge.
					graph.connect(nodes[black], nodes[white]);
					graph.connect(nodes[white], nodes[black]);
					break;
			}
		}
	}

	std::clock_t start = std::clock();

	for (unsigned int place = 1; !graph.empty(); ++place)
	{
		const Graph::Node* node = graph.highestNetDegree();
		std::cout << place << ". " << node->name << std::endl;
		graph.remove(node);
	}

	std::clock_t end = std::clock();
	std::cout << "Took " << ((double)(end - start)) / CLOCKS_PER_SEC
	          << " seconds." << std::endl;
}
