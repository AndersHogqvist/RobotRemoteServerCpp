*** Settings ***
Library    Remote    http://127.0.0.1:8270

*** Test Cases ***
Echo Example
    ${result}=    Echo    hello
    Should Be Equal    ${result}    hello

Add Example
    ${result}=    Add    1    2    3.5

Count Items in Directory
    ${items1} =    Count Items In Directory    ${CURDIR}
    ${items2} =    Count Items In Directory    ${TEMPDIR}
    Log    ${items1} items in '${CURDIR}' and ${items2} items in '${TEMPDIR}'

Failing Example
    Strings Should Be Equal    Hello    Hello
    Strings Should Be Equal    not      equal