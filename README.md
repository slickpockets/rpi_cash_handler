### Cash recycler (Innovative Technology nv200 + smart payout) ###
## Works with redis to store denomination information in an accessable and friendly manner

payout - executable that wil payout from the cash recycler

route - route cash to cash recycler or cash box

command - performs activation/deactivation of the cash recycler and other sundrys

run_scanner_driver.py - old interface to pyramid techniologies apex 7000 

gcc *.c files and static link to hirdis and lib 


##currently route now makes note of last slot
need to add redis to payout with amount * -1 incrby 

***migrated from personal git server***
