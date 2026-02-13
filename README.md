# Robot Remote Server C++

A small C++20 library that implements the Robot Framework Remote Library XML-RPC interface. It lets a host C++ application register keywords and expose them over XML-RPC, similar to `nrobot-server`.

## Build

```
cmake -S . -B build
cmake --build build
```

## Example

Run the sample host:

```
./build/robot_remote_example
```

Then use a Robot Framework test with the Remote library:

```
*** Settings ***
Library    Remote    http://127.0.0.1:8270

*** Test Cases ***
Echo Example
    ${result}=    Echo    hello
    Should Be Equal    ${result}    hello

Add Example
    ${result}=    Add    1    2    3.5
    Should Be Equal    ${result}    6.5
```

## API Overview

- `RemoteServer` hosts the XML-RPC server and dispatches keyword calls.
- Register keywords with `set_keywords`, providing name, docs, argument spec, and a handler.

The XML-RPC interface methods implemented are:
- `get_keyword_names`
- `get_keyword_arguments`
- `get_keyword_documentation`
- `get_library_information`
- `run_keyword`

## Documentation

Generate API docs with Doxygen (if installed):

```
cmake -S . -B build
cmake --build build --target docs
```

HTML output will be in `docs/html`.
