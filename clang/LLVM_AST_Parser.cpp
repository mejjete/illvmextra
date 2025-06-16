#include <iostream>
#include <fstream>
#include <regex>
#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/program_options.hpp>

using namespace boost;
namespace po = boost::program_options;

using VertexProperty = property<vertex_name_t, std::string>;
using ASTVisGraph = adjacency_list<vecS, vecS, directedS, VertexProperty>;

struct CustomLabelWriter 
{
    explicit CustomLabelWriter(ASTVisGraph& g) : graph(g) {}

    template <typename Vertex>
    void operator()(std::ostream& out, const Vertex& v) const 
    { 
        std::string label = get(vertex_name, graph, v);
        std::replace(label.begin(), label.end(), '"', '\'');
        out << "[label=\"" << label << "\"]"; 
    }

    ASTVisGraph& graph;
};

/**
 *  There's basically two types of a filter, the ones that affect the label of the node, their name start with DF.
 *  They basically control the display options of the node, i.e disable or enable the optional node information.
 *  
 *  The other begin with TF, it modifies the AST tree itself either by transforming it or by omitting some of the 
 *  branches of the tree that does not seem to be relevant. 
 */
enum 
{
    NO_FT = 0x0,

    // Display Filters [0x1 - 0x80]
    DF_NO_NODE_ID   = 0x1,                              // Disable displaying internal node ID 
    DF_NO_LOC       = 0x2,                              // Disable locations
    DF_NO_ERRS      = 0x4,                              // Disable any notation about errors during AST parsing
    
    // Tree Filters [0x100 - 0x800]
    TF_NO_IMPLICIT  = 0x100,                            // Ommits the implicit compiler declarations

    // Preset of filters
    PR_FANCY = DF_NO_NODE_ID | DF_NO_ERRS | DF_NO_LOC | TF_NO_IMPLICIT
};

ASTVisGraph ParseLLVMAST(std::ifstream& iFile, int FormatArgs);
std::string& FilterLine(std::string& Line, int FormatArgs);

int main(int argc, char **argv)
{
    po::options_description desc("Available options");
    desc.add_options()
        ("help,h", "Show help message")
        ("format,f", po::value<std::string>(), "Apply filters to incomming LLVM AST")
        ("list,l", "Show available filters")
        ("output-file,o", po::value<std::string>(), "Specify output Graphviz file")
        ("input-file", po::value<std::string>(), "Specify input LLVM AST file");
    
    po::positional_options_description p;
    p.add("input-file", -1);

    int FormatArgs = NO_FT;

    try 
    {   
        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
        po::notify(vm);

        if(vm.count("help"))
        {
            std::cout << desc << std::endl;
            return EXIT_SUCCESS;
        } 
        else if(vm.count("list"))
        {
            std::cout << "\tno-node-id    - Removes internal node identification number\n";
            std::cout << "\tno-loc        - Removes any location information\n";
            std::cout << "\tno-errors     - Removes error message\n";
            std::cout << "\tno-implicit   - Removes any node containing 'implicit' annotation\n";
            std::cout << "\tfancy         - A set of filters aimed at providing clear AST visualization\n";
            return EXIT_SUCCESS;
        } 
        else if(vm.count("format"))
        {
            std::istringstream FormatStream(vm["format"].as<std::string>());
            std::string Format;
            
            while(FormatStream >> Format)
            {
                if(Format == "no-node-id")
                    FormatArgs |= DF_NO_NODE_ID;
                if(Format == "no-loc")
                    FormatArgs |= DF_NO_LOC;        
                if(Format == "no-errors")           
                    FormatArgs |= DF_NO_ERRS;       
                if(Format == "no-implicit")
                    FormatArgs |= TF_NO_IMPLICIT;   
                if(Format == "fancy")
                    FormatArgs |= PR_FANCY;         
                
                if(FormatArgs == NO_FT)
                    throw po::error("No such format option: " + Format);
            }
        }

        const auto& InputFileName = vm["input-file"].as<std::string>();
        const auto& OutputFileName = vm["output-file"].as<std::string>();

        std::ifstream iFile(InputFileName, std::ios::binary);
        if(!iFile.is_open())
            throw std::runtime_error("ERROR opening input file: " + InputFileName);
        
        std::ofstream oFile(OutputFileName, std::ios::binary);
        if(!oFile.is_open())
            throw std::runtime_error("ERROR opening output file: " + OutputFileName);
        
        auto Graph = ParseLLVMAST(iFile, FormatArgs);
        write_graphviz(oFile, Graph, CustomLabelWriter(Graph));

    } catch(const po::error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Use '--help' to see valid options" << std::endl;
    } catch(const std::runtime_error& re) {
        std::cerr << re.what() << std::endl;
        std::cerr << "Specify valid input and output files" << std::endl;
    }
};

ASTVisGraph ParseLLVMAST(std::ifstream& iFile, int FormatArgs) 
{ 
    ASTVisGraph Graph;
    std::string Line;

    // Read the TranslationUnitDecl because it does not have formatting characters
    std::getline(iFile, Line);
    std::vector<typename ASTVisGraph::vertex_descriptor> NodeArray;
    NodeArray.push_back(add_vertex(Graph));
    put(vertex_name, Graph, NodeArray[0], Line);

    while(std::getline(iFile, Line))
    {
        if((FormatArgs & TF_NO_IMPLICIT) == TF_NO_IMPLICIT)
        {
            // Skip the sub-tree that satisfies following condition
            while(Line.find("implicit") != std::string::npos)
            {
                std::size_t ParentIndents = 0;
                for(auto iter : Line)
                {
                    if(isalpha(iter))
                        break;
                    ParentIndents++;
                }

                std::size_t ParentLayout = ParentIndents / 2;

                while(std::getline(iFile, Line))
                {
                    std::size_t ChildIndents = 0;
                    for(auto iter : Line)
                    {
                        if(isalpha(iter))
                            break;
                        ChildIndents++;
                    }

                    std::size_t ChildLayout = ChildIndents / 2;

                    if(ChildLayout == ParentLayout)
                        break;
                }
            }
        }

        //---------------------------------------------------------------------
        //  At this point, all tree transformations are done
        // ---------------------------------------------------------------------

        // Skip any occurences of the formatting characters
        auto LabelIter = Line.find_first_not_of(" -|`");

        // Extract actual label frosm the orginial line to get only node's label
        std::string Label = std::string(Line.begin() + LabelIter, Line.end());
        Label = FilterLine(Label, FormatArgs);

        // Analyze the Line trying to figure out the node's parent
        std::size_t Indents = 0;
        for(auto iter : Line)
        {
            if(isalpha(iter))
                break;
            Indents++;
        }

        std::size_t Layout = Indents / 2;

        // Resize the vector of nodes to the parent's of the currect node
        if(Layout < NodeArray.size())
            NodeArray.resize(Layout);

        // Add new node to the appropiate place
        auto NewVertex = add_vertex(Graph);
        put(vertex_name, Graph, NewVertex, Label);
        NodeArray.push_back(NewVertex);
        add_edge(NodeArray[Layout - 1], NewVertex, Graph);
    }

    return Graph;
};

std::string& FilterLine(std::string& Line, int FormatArgs) 
{ 
    auto Iter = std::string::npos;

    // Removes internal hexademical node representation 
    if((FormatArgs & DF_NO_NODE_ID) == DF_NO_NODE_ID)
    {
        if((Iter = Line.find("0x")) != std::string::npos)
        {
            // +1 means we would like to remove also a tralling 0
            auto HexEnd = Line.find_first_of(" ", Iter);
            Line.erase(Line.begin() + Iter, Line.begin() + HexEnd + 1);
        }
    }

    // Removes '<<invalid sloc>> <invalid sloc>'
    if((FormatArgs & DF_NO_LOC) == DF_NO_LOC)
    {
        std::string InvalidSloc = "<<invalid sloc>> <invalid sloc>";
        if((Iter = Line.find(InvalidSloc)) != std::string::npos)
            Line.erase(Iter, InvalidSloc.length() + 1);
    }

    // Removes 'contains-errors'
    if((FormatArgs & DF_NO_ERRS) == DF_NO_ERRS)
    {
        std::string ContainsErrors = "contains-errors";
        if((Iter = Line.find(ContainsErrors)) != std::string::npos)
            Line.erase(Iter, ContainsErrors.length() + 1);
    }
    
    return Line;
};
