## Part 2: Putting it all together, BLINK DB


To benchmark the blink db
```redis-benchmark -h 127.0.0.1 -p 9001 -t set,get,del -n 100000 -c 100 -e -r 1000;```


### 1. How to run ?

a. build the code

```
make
```

b. run the blinkdb server

```
make run_blinkdb
```

c. run the blinkdb client

```
make run_blinkdb_cli
```


### 2. How to get design doc ?

go to the file

```
/docs/design_doc.md
```

### 3. How to get doxyzen_doc pdf ?

go to the file

```
docs/doxyzen_doc.pdf
```

### 3. How to get doxyzen_doc pdf ?

go to the file

```
docs/html/index.html
```

### 4. How to generate doxyzen ?

```
doxygen Doxyfile
```

