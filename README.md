# Multi-thread Prime number
 Multi threaded program to read .txt input and determine if numbers are prime or not
 
 ## Compile with:
 ```
 make
 ```
 
 ## Run with:
 ```
 ./detectPrimes <# of threads>  < ./tests/<testfile.txt>
 ```
 
 ## Example with Output:
 ```
 ./detectPrimes 5 < ./tests/example.txt
 ```
 Where the simulator would run with 5 concurrent threads
 
 ```
 Using 5 threads.
 Identified 3 primes:
   3 19 1033
 Finished in 0.00000s
 ```
