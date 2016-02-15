echo "Removing gtthread.a"
rm gtthread.a
echo "Creating archive gtthread.a"
ar -cvq gtthread.a *.o
echo "Running ranlib on gtthread.a"
ranlib gtthread.a
