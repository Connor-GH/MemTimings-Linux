# MemTimings-Linux
![](https://github.com/Connor-GH/MemTimings-Linux/blob/main/logo.png?raw=true)


Find your memory Timings of a Ryzen Zen2+ cpu under linux!
**Important** **Your binary needs to be called "timings", otherwise, have a fun time using** 
**sed to replace the word "timings" with the name that you chose**
The program is currently in beta, so expect some bugs.
It requires sudo/root privilages, just like ZenTimings on windows.
It works by using Cyring's SMU address reader, and parsing the 
binary strings into memory timing numbers.
Cyring's Github: https://github.com/cyring
Zentiming's Github: https://github.com/irusanov/ZenTimings
<img src="https://i.imgur.com/sm2VGuO.png" alt="MemTimings1" align="right" height="387px">
## To install:
* ``git clone https://github.com/Connor-GH/MemTimings-Linux``
* ``cd MemTimings-Linux-main/``
* ``gcc -o timings timings.c``
* ``./timingsaddon``
