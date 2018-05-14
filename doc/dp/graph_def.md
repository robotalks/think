# DataPipeline Graph Definition

## Example

```
# the comment

arg model = graph, port = 2052, topic

in input = udp use builtin.ingress.udp{ port: $port }
in a, b = something use some-type { }

id = imgid(input) use builtin.imageid{}
size, pixels = decode(input) use builtin.image_decode{}
op objects = detect(pixels) use ssdmobilenet{model:$model}
publish(id, size, objects) use mqttpub{topic:$topic}

```

## Syntax

A graph definition contains a list of statements,
each starting with a keyword,
followed by one or more definitions of elements:

- `arg`: define arguments, which can be provided at graph creation time
- `in`: define ingresses
- `op`: (keyword optional) define operations

A statement is finished by a newline,
unless a comma `,` follows the last definition of elements.
And it will starts a definition of the next elements, in the same statement.

For example

```
arg input, id
```

or

```
arg input,
    id
```

### Arguments

```
arg name1, name2 = default_value, ...
```

The keyword `arg` defines one or more arguments which must be provided when
a graph is created.
The value of an argument can be referenced as `$name` in the `params` part of
_operator factory_ or _ingress factory_.

An argument can be defined as `name`, or `name = default_value`.
The former doesn't have a default value, and must be provided to create a graph.

### Ingress

Ingresses are the elements which provides input variables to kick off the
execution of the graph.

```
in var1, var2, ... = name use ingress_type {
    key1: value1,
    key2: value2,
}
```

Similar to `op`, first comes the list of input variables the ingress will fill in.
After `=`, the name of ingress is defined, and after `use`, `ingress_type` specifies
the name of ingress factory, and brackets `{` and `}` encapsulate key/value pairs
as parameters for the factory to create the ingress.

Ingress in a graph definition is optional.
When ingresses are present, a `graph_dispatcher` can be created and run immediately
instead of graph.

### Operators

```
op out_var1, out_var2, ... = op_name(in_var1, in_var2, ...) use op_type {
    key1: value1,
    key2: value2,
    ...
}
```

Keyword `op` (optional) defines an operator,
by specifying the list of output variables,
name of the operator, input variables, type of the operator and parameters
for the operator factory to create the operator.

`op_type` specifies the operator factory name, and brackets `{` and `}`
encapsulate key/value pairs as parameters for the factory to create the operator.

To define operators without output variables:

```
op_name(in_var1, in_var2 ...) use op_type { ... }
```

And without input variables:

```
op_name() use op_type { ... }
```
