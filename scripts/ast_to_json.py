import ast
import json
import sys

# Map python calls to Block Opcodes
OP_MAP = {
    'move_axis_abs': 'move_axis_abs',
    'axis_move': 'move_axis_abs',  # Alias
    'sleep_ms': 'sleep_ms',
    'set_do': 'set_do',
    'print': 'print',
    'log_message': 'print', # Alias
    'log': 'print',         # Alias
    'set_twist': 'set_twist' # New Opcode
}

class GraphBuilder(ast.NodeVisitor):
    def __init__(self):
        self.nodes = []
        self.wires = []
        self.last_id = None
        self.id_counter = 1
        self.y_cursor = 100

    def add_node(self, opcode, params):
        nid = self.id_counter
        self.id_counter += 1
        
        node = {
            "id": nid,
            "op": opcode,
            "x": 200,
            "y": self.y_cursor,
            "params": params
        }
        self.nodes.append(node)
        self.y_cursor += 150
        
        if self.last_id:
            self.wires.append({"from": self.last_id, "to": nid})
        self.last_id = nid

    def visit_Call(self, node):
        # We look for host.xyz() OR host_api.xyz() calls
        is_host = False
        if isinstance(node.func, ast.Attribute):
             if isinstance(node.func.value, ast.Name) and node.func.value.id in ['host', 'host_api']:
                 is_host = True
        
        if is_host:
            func_name = node.func.attr
            if func_name in OP_MAP:
                opcode = OP_MAP[func_name]
                # Extract args
                vals = []
                for a in node.args:
                    if isinstance(a, ast.Constant): vals.append(a.value)
                    elif isinstance(a, ast.UnaryOp) and isinstance(a.op, ast.USub) and isinstance(a.operand, ast.Constant):
                        vals.append(-a.operand.value) # Handle negative numbers
                    else: vals.append(0) # Fallback
                
                # Manual Mapping based on Opcode Schema
                params = {}
                if opcode == 'move_axis_abs': params = {'axis':vals[0], 'pos':vals[1], 'vel':vals[2] if len(vals)>2 else 50}
                elif opcode == 'sleep_ms': params = {'ms':vals[0]}
                elif opcode == 'set_do': params = {'port':vals[0], 'val':vals[1]}
                elif opcode == 'set_twist': params = {'vx':vals[0], 'vy':vals[1], 'wz':vals[2]}
                elif opcode == 'print': params = {'msg': str(vals[0])}
                
                self.add_node(opcode, params)
        
        # Handle time.sleep specifically
        if isinstance(node.func, ast.Attribute) and isinstance(node.func.value, ast.Name) and node.func.value.id == 'time' and node.func.attr == 'sleep':
             # time.sleep(seconds) -> sleep_ms(ms)
             args = node.args
             if args and isinstance(args[0], ast.Constant):
                 sec = args[0].value
                 ms = int(sec * 1000)
                 self.add_node('sleep_ms', {'ms': ms})
        
        # Handle print separately
        if isinstance(node.func, ast.Name) and node.func.id == 'print':
             vals = []
             for a in node.args:
                 if isinstance(a, ast.Constant): vals.append(str(a.value))
             self.add_node('print', {'msg': " ".join(vals)})

    def visit_Expr(self, node):
        self.visit(node.value)

def parse(code):
    tree = ast.parse(code)
    builder = GraphBuilder()
    for stmt in tree.body:
        builder.visit(stmt)
    return {"nodes": builder.nodes, "wires": builder.wires}

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("{}")
        sys.exit(0)
    
    try:
        with open(sys.argv[1], 'r', encoding='utf-8') as f:
            code = f.read()
    
        res = parse(code)
        print(json.dumps(res))
    except Exception as e:
        # Return error as JSON so backend can relay it
        print(json.dumps({"error": str(e)}))
        sys.exit(1)
