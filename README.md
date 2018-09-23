My implementation of networks classes assignment, which had these requirements:

A server to which one can connect with telnet program (which serves as a client). After accepting the client connection server prints on stdout that the client has connected and client prints the main menu:

> Option A  
> Option B  
> End

In the client one can use up and down arrow keys to navigate the menu and choose an option with an enter key. End option makes server end the connection with client, print on stdout that the connection has ended and wait for another client connection.
A option makes the client print out A below. B option shows the menu:

> Option B1  
> Option B2  
> Back

B1 and B2 print, respectively, B1 and B2 below. Back makes the client go back to main menu.

Server is launched with one argument, a decimal number of the port of the connections (other format of arguments closes program with an error communicat). The communication is TCP-based with telnet protocol and uses Linux socket interface.

[Archived original assignment requirements, in PL](https://web.archive.org/web/20180923121722/http://students.mimuw.edu.pl/~nk385744/folders/SIK/zadania/zadanie_1.txt)
