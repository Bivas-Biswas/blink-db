## SOME HANDY COMMAND

To benchmark the redis
```redis-benchmark -t set,get,del -n 100000 -c 10 -e -r 1000;```

To benchmark the blink db
```redis-benchmark -h 127.0.0.1 -p 9001 -t set,get,del -n 100000 -c 100 -e -r 1000;```

To kill all the processes with name blink_ser
```ps aux | grep blink_ser | awk '{print $2}' | xargs kill -9```