import re
import pydot

# Regex patterns
ANSI_ESCAPE = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')  # Colors
SLOC_PATTERN = re.compile(r'<[^>]*>')                               # SLOC
POINTER_PATTERN = re.compile(r'0x[0-9a-fA-F]+')                     # Memory pointers

def clean_line(line):
    """ Remove ANSI escape sequences, SLOC, and memory pointers """
    line = ANSI_ESCAPE.sub('', line)
    line = SLOC_PATTERN.sub('', line)
    line = POINTER_PATTERN.sub('', line)
    return line.strip()

def parse_ast(file):
    graph = pydot.Dot("AST", graph_type="digraph", bgcolor="white")
    node_stack = []
    node_counter = 0

    with open(file, 'r') as f:
        previous_indent = 0
        parent = None

        for line in f:
            cleaned = clean_line(line)
            if not cleaned:
                continue

            current_indent = line.count("| ")
            label = cleaned.strip("| ")

            node_name = f"node{node_counter}"
            node_counter += 1
            node = pydot.Node(node_name, label=label, shape="box", fontsize="10", style="filled", fillcolor="#D5E8D4")
            graph.add_node(node)

            if current_indent > previous_indent:
                node_stack.append(parent)
            elif current_indent < previous_indent:
                node_stack = node_stack[:current_indent]

            if node_stack:
                graph.add_edge(pydot.Edge(node_stack[-1], node))

            parent = node
            previous_indent = current_indent

    return graph

if __name__ == "__main__":
    filename = "main.ast"  # AST file from Clang
    graph = parse_ast(filename)
    graph.write("main.dot")
    print("AST visualization saved to main.dot")

    import os
    os.system("dot -Tpng main.dot -o main.png")
    print("AST image saved to main.png")
