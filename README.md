# db-query-proxy

This project is some kind of MITM proxy for modifying SQL
queries.
It may be useful if you have a proprietary software or if
you can't modify SQL queries produced by application.

So if you're a DBA (Database Administrator), you can modify
and tune application queries if you know exactly what you're
doing.

## Project status

Currently only PostgreSQL protocol is supported.
In case of users interest, other database protocols may be
implemented also like MySQL, MSSQL, Oracle, etc.

Binary packages are also not provided yet.

Contact me if you need additional info.

## Build

You should have Qt4 or Qt5 installed.

For example, on CentOS 7:

    # install qt
    sudo yum install qt qt-devel
    # clone git repo
    git clone https://github.com/nickkarikh/db-query-proxy.git
    # cd into repo dir
    cd db-query-proxy
    # create bin dir
    mkdir bin
    # cd into sources dir
    cd src
    # create makefile
    qmake-qt4
    # build
    make
    # cd to bin dir
    cd ../bin
    # copy example config file to /etc
    sudo cp ../etc/db-query-proxy.conf
    # run
    ./db-query-proxy

## Configuration

Example configuration file is provided in the `etc` subdirectory.

Quick example:

    query = SELECT \* FROM tablename WHERE (.+)
    action = rewrite
    rewrite = SELECT * FROM tablename WHERE (col3 >= '$(now-1M)') AND $(1)

psql (connecting to db-query-proxy instead of direct connection to PostgreSQL):

    psql -U postgres -h localhost -p 5433 dbname
    dbname=# SELECT * FROM tablename WHERE id=1;

Log output:

    debug: ORIGINAL query: SELECT * FROM tablename WHERE id=1;
    debug: MODIFIED query (rule 1): SELECT * FROM tablename WHERE (col3 >= '2018-11-12 11:25:23.0+00') AND id=1;
