#include <iostream>
#include <fstream>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>

using namespace boost;
using VertexProperty = property<vertex_name_t, std::string>;
using ASTVisGraph = adjacency_list<vecS, vecS, directedS, VertexProperty>;

struct CustomLabelWriter 
{
    explicit CustomLabelWriter(ASTVisGraph& g) : graph(g) {}

    template <typename Vertex>
    void operator()(std::ostream& out, const Vertex& v) const 
    { out << "[label=\"" << get(vertex_name, graph, v) << "\"]"; }

    ASTVisGraph& graph;
};

ASTVisGraph ParseLLVMAST(std::ifstream& ifile);
std::string FilterLine(std::string& Line);

int main(int argc, char **argv)
{
    if(argc < 3)
        throw std::runtime_error("Invalid number of arguments\n");

    std::ifstream ifile(argv[1], std::ios::binary);
    if(!ifile.is_open())
        throw std::runtime_error("ERROR opening input file: " + std::string(argv[1]));
    
    std::ofstream ofile(argv[2], std::ios::binary);
    if(!ofile.is_open())
        throw std::runtime_error("ERROR opening output file: " + std::string(argv[2]));
    
    auto Graph = ParseLLVMAST(ifile);
    write_graphviz(ofile, Graph, CustomLabelWriter(Graph));
};

ASTVisGraph ParseLLVMAST(std::ifstream& ifile) 
{ 
    ASTVisGraph Graph;
    std::string Line;

    // Read the TranslationUnitDecl because it does not have formatting characters
    std::getline(ifile, Line);
    std::vector<typename ASTVisGraph::vertex_descriptor> NodeArray;
    NodeArray.push_back(add_vertex(Graph));
    put(vertex_name, Graph, NodeArray[0], Line);

    while(std::getline(ifile, Line))
    {
        // Skip any occurences of the formatting characters
        auto LabelIter = Line.find_first_not_of(" -|`");

        // Extract actual label frosm the orginial line to get only node's label
        std::string Label = std::string(Line.begin() + LabelIter, Line.end());
        Label = FilterLine(Label);

        // Analyze the Line trying to figure out the node's parent
        std::size_t NumberOfSpaces = 0;
        for(auto iter : Line)
        {
            if(iter == '-')
                break;
            else if(iter == ' ')
                NumberOfSpaces++;
        }

        std::size_t NodeDegree = ((NumberOfSpaces + 1) / 2) + 1;

        // Resize the vector of nodes to the parent's of the currect node
        if(NodeDegree < NodeArray.size())
            NodeArray.resize(NodeDegree);

        // Add new node to the appropiate place
        auto NewVertex = add_vertex(Graph);
        put(vertex_name, Graph, NewVertex, Label);
        NodeArray.push_back(NewVertex);
        add_edge(NodeArray[NodeDegree - 1], NewVertex, Graph);
    }

    return Graph;
};

std::string FilterLine(std::string& Line) 
{ 
    std::string toReplace = "TypedefDecl";

    auto pos = Line.find(toReplace);
    if(pos != std::string::npos)
        Line.replace(pos, toReplace.length(), "typedef");
    
    return Line;
};