BFTDoc: A Byzantine-fault-tolerant distributed document editor
======
Designed and implemented by Alex Epstein (/gunass) & Yuexing Huang (/ame) for COMP90020 Distributed Algorithms at the University of Melbourne. This project received 45/45 marks in total. BFTDoc is based (incompletely) on Copeland & Zhong's [Tangaroa](https://github.com/chrisnc/tangaroa) and we are indebted to these authors. 

The service
-----------
This is a service that allows you to run multiple (local) replicas of a distributed document editor.
By running the client, users can append or delete characters from the document, and request to view the state of the document. 

The distributed system is resistant to many Byzantine faults. These may include nodes issuing fradulent requests, impersonating other nodes, etc. 

Replicas should be initiated first from the command line.

``$ ./bftdoc 4000``

starts a replica on port 4000. 

The client is initiated similarly:

``$ ./bftdoc 5000 client``

``$ Give a command (append, delete, view, elect):``

``$ >>> append``

``$ What would you like to append?``

``$ >>> Hello, World!``

``$ Successfully appended "Hello, World!" to the document.``

This above example shows the interactive nature of the client. Four commands are available to users. "append" and "delete" require a second argument, which is a string or a number respectively. "view" returns a copy of the document and "elect" forces an election. 

Replicas may be initiated as Byzantine. This means they have a certain probability of performing irrational actions such as changing state, maliciously modifying or inventing messages, or impersonating other nodes.

``$ ./bftdoc 4003 byzantine``

This creates a Byzantine replica on port 4003. Using this feature, the integrity of the system can be tested. 




