BFTDOC, a Byzantine-fault-tolerant distributed document editor
By Alex Epstein & Yuexing Huang
Based on Copeland & Zhong's BFT RAFT (2016)

This is a service that allows you to run multiple (local) replicas of 
a distributed document editor.
By running the client, users can append or delete characters from the 
document, and request to view the state of the document. 

The distributed system is resistant to Byzantine faults. These may include 
nodes issuing fradulent requests, impersonating other nodes, etc. 

Replicas are initiated from the command line:
$ ./bftdoc 4000
starts a replica on port 4000. 

The client is initiated similarly:
$ ./bftdoc 5000 client
$ Give a command (append, delete, view, elect):
$ >>> append
$ What would you like to append?
$ >>> Hello, World!
$ Successfully appended "Hello, World!" to the document.

This above example shows the interactive nature of the client.
Four commands are available to users. "append" and "delete" require a second
argument, which is a string or a number respectively.

Replicas may be initiated as Byzantine. This means they have a certain
probability of performing irrational actions such as changing state,
maliciously modifying or inventing messages, or impersonating other nodes.
$ ./bftdoc 4003 byzantine
This creates a Byzantine replica on port 4003. Using this feature, the
integrity of the system can be tested. 

Byzantine faults are protected against primarily by signatures. The system
uses a very primitive notion of cryptography as a demonstration. However,
the algorithm itself can be shown to be resilient to all manner of Byzantine
failures. 




