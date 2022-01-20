# MemTimings-Linux
Find your memory Timings of a Ryzen Zen2+ cpu under linux!
**Important** **Your binary needs to be called "timings", otherwise, have a fun time using** 
**sed to replace the word "timings" with the name that you chose**
The program is currently in beta, so expect some bugs.
It requires sudo/root privilages, just like ZenTimings on windows.
It works by using Cyring's SMU address reader, and parsing the 
binary strings into memory timing numbers.
Cyring's Github: https://github.com/cyring
Zentiming's Github: https://github.com/irusanov/ZenTimings
