neo4j-client
============


About
-----

neo4j-client is a command shell (CLI) for Neo4j. It supports secure connections
to Neo4j server, sending of statements (including multiline statements),
persistent command history, and rendering of results to tables or CSV.

neo4j-client utilizes the [Bolt Network Protocol](https://boltprotocol.org), and
will work with any server that supports Bolt.

For more details, see [the project page](https://neo4j-client.net) and
the [FAQ](https://github.com/cleishm/libneo4j-client/wiki/FAQ).


Requirements
------------

neo4j-client is known to work on GNU/Linux, Mac OS X and FreeBSD. It
requires neo4j 3.0.0 or later.


Getting Started
---------------

If you're using Mac OS X, neo4j-client can be installed using homebrew:

```console
$ brew install cleishm/neo4j/neo4j-client
```

If you're using Ubuntu, neo4j-client can be install using APT:

```console
$ sudo add-apt-repository ppa:cleishm/neo4j
$ sudo apt-get update
$ sudo apt-get install neo4j-client libneo4j-client-dev
```

There are also packages available for other platforms, including
[Debian](https://packages.debian.org/source/sid/libneo4j-client),
[Fedora](https://build.opensuse.org/package/binaries/home:cleishm/libneo4j-client/Fedora_23),
[CentOS](https://build.opensuse.org/package/binaries/home:cleishm/libneo4j-client/CentOS_7) and
[openSUSE](https://build.opensuse.org/package/binaries/home:cleishm/libneo4j-client/openSUSE_Tumbleweed).

Otherwise, please see [Building](#building) below.


neo4j-client Usage
------------------

Example interactive usage:

```console
$ neo4j-client -u neo4j localhost
The authenticity of host 'localhost:7687' could not be established.
TLS certificate fingerprint is ded0fd2e893cd0b579f47f7798e10cb68dfa2fd3bc9b3c973157da81bab451d74f9452ba99a9c5f66dadb8a360959e5ebd8abb2d7c81230841e60531a96d268.
Would you like to trust this host (NO/yes/once)? yes
Password: *****
neo4j> :help
Enter commands or cypher statements at the prompt.

Commands always begin with a colon (:) and conclude at the end of the line,
for example `:help`. Statements do not begin with a colon (:), may span
multiple lines, are terminated with a semi-colon (;) and will be sent to
the Neo4j server for evaluation.

Available commands:
:quit                  Exit the shell
:connect '<url>'       Connect to the specified URL
:connect host [port]   Connect to the specified host (and optional port)
:disconnect            Disconnect the client from the server
:export                Display currently exported parameters
:export name=val ...   Export parameters for queries
:unexport name ...     Unexport parameters for queries
:reset                 Reset the session with the server
:set                   Display current option values
:set option=value ...  Set shell options
:unset option ...      Unset shell options
:status                Show the client connection status
:help                  Show usage information
:format (table|csv)    Set the output format
:width (<n>|auto)      Set the number of columns in the table output

For more information, see the neo4j-client(1) manpage.
neo4j>
neo4j> :status
Connected to 'neo4j://neo4j@localhost:7687'
neo4j>
neo4j> MATCH (n:Person) RETURN n LIMIT 3;
+----------------------------------------------------------------------------+
| n                                                                          |
+----------------------------------------------------------------------------+
| (:Person{born:1964,name:"Keanu Reeves"})                                   |
| (:Person{born:1967,name:"Carrie-Anne Moss"})                               |
| (:Person{born:1961,name:"Laurence Fishburne"})                             |
+----------------------------------------------------------------------------+
neo4j>
neo4j> :set
 echo=off           // echo non-interactive commands before rendering results
 insecure=no        // do not attempt to establish secure connections
 format=table       // set the output format (`table` or `csv`).
 outfile=           // redirect output to a file
 username="neo4j"   // the default username for connections
 width=auto         // the width to render tables (`auto` for term width)
neo4j>
neo4j> :quit
$
```

Example non-interactive usage:

```console
$ echo "MATCH (n:Person) RETURN n.name AS name, n.born AS born LIMIT 3" | \
        neo4j-client -u neo4j -P localhost > result.csv
Password: *****
$
$ cat result.csv
"name","born"
"Keanu Reeves",1964
"Carrie-Anne Moss",1967
"Laurence Fishburne",1961
$
```

Evaluating source files, e.g.:

```console
$ cat query.cyp
:set echo
:export name='Emil'

// Create a person node if it doesn't exist
begin;
MERGE (:Person {name: {name}});
commit;

// return the total number of people
MATCH (n:Person)
RETURN count(n);
$
$ neo4j-client -u neo4j -p pass -o result.out -i query.cyp
$ cat result.out
+:export name='Emil'
+begin;
+MERGE (:Person {name: {name}});
Nodes created: 1
Properties set: 1
Labels added: 1
+commit;
+MATCH (n:Person)
 RETURN count(n);
"count(n)"
137
$
```


libneo4j-client
---------------

libneo4j-client is a client library for Neo4j, written in C, and intended as a
foundation on which basic tools and drivers for various languages may be built.
libneo4j-client takes care of all the detail of establishing a session with a
Neo4j server, sending statements for evaluation, and retrieving results.

libneo4j-client provides a single C header file, `neo4j-client.h`, for
inclusion in source code using the libneo4j-client API. The API is described in
the [API Documentation](#api_documentation).

libneo4j-client can be included in your project by linking the library at
compile time, typically using the linking flags
`-lneo4j-client -lssl -lcrypto -lm`.  Alternatively, libneo4j-client ships with
a [pkg-config]( https://wiki.freedesktop.org/www/Software/pkg-config/)
description file, enabling you to obtain the required flags using
`pkg-config --libs libneo4j-client`.


API Documentation
-----------------

API documentation for the latest release is available at
[https://neo4j-client.net/doc/latest/neo4j-client\_8h.html](
[https://neo4j-client.net/doc/latest/neo4j-client_8h.html).

Documentation can be built using `make doc`, which will use doxygen to generate
documentation and output it into the `doc/` directory of the libneo4j-client
source tree. See [Building](#building) below.


Example
-------

```C
#include <neo4j-client.h>
#include <errno.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    neo4j_client_init();

    /* use NEO4J_INSECURE when connecting to disable TLS */
    neo4j_connection_t *connection =
            neo4j_connect("neo4j://user:pass@localhost:7687", NULL, NEO4J_INSECURE);
    if (connection == NULL)
    {
        neo4j_perror(stderr, errno, "Connection failed");
        return EXIT_FAILURE;
    }

    neo4j_result_stream_t *results =
            neo4j_run(connection, "RETURN 'hello world'", neo4j_null);
    if (results == NULL)
    {
        neo4j_perror(stderr, errno, "Failed to run statement");
        return EXIT_FAILURE;
    }

    neo4j_result_t *result = neo4j_fetch_next(results);
    if (result == NULL)
    {
        neo4j_perror(stderr, errno, "Failed to fetch result");
        return EXIT_FAILURE;
    }

    neo4j_value_t value = neo4j_result_field(result, 0);
    char buf[128];
    printf("%s\n", neo4j_tostring(value, buf, sizeof(buf)));

    neo4j_close_results(results);
    neo4j_close(connection);
    neo4j_client_cleanup();
    return EXIT_SUCCESS;
}
```


Building
--------

To use neo4j-client or libneo4j-client, consider installation using the package
management system for your operating system (currently
[Mac OS X](#getting_started),
[Debian](https://packages.debian.org/source/sid/libneo4j-client),
[Ubuntu](#getting_started),
[Fedora](https://build.opensuse.org/package/binaries/home:cleishm/libneo4j-client/Fedora_23),
[CentOS](https://build.opensuse.org/package/binaries/home:cleishm/libneo4j-client/CentOS_7) and
[openSUSE](https://build.opensuse.org/package/binaries/home:cleishm/libneo4j-client/openSUSE_Tumbleweed)).

If neo4j-client is not available via your package management system,
please [download the latest release](
https://github.com/cleishm/libneo4j-client/releases), unpack and then:

```console
$ ./configure
$ make clean check
$ sudo make install
```

libneo4j-client requires OpenSSL, although this can be disabled by invoking
configure with `--without-tls`.

neo4j-client also requires some dependencies to build, including
[libedit](http://thrysoee.dk/editline/) and
[libcypher-parser](https://git.io/libcypher-parser). If these are not available,
just the library can be built (without neo4j-client), by invoking configure
with `--disable-tools`.

Building from the GitHub repository requires a few extra steps. Firstly, some
additional tooling is required, including autoconf, automake and libtool.
Assuming these are available, to checkout from GitHub and build:

```console
$ git clone https://github.com/cleishm/libneo4j-client.git
$ cd libneo4j-client
$ ./autogen.sh
$ ./configure
$ make clean check
$ sudo make install
```

If you encounter warnings or errors during the build, please report them at
https://github.com/cleishm/libneo4j-client/issues. If you wish to proceed
dispite warnings, please invoke configure with the `--disable-werror`.

NOTE: Recent versions of Mac OS X ship without the OpenSSL header files, and
autoconf doesn't pick this up (yet). If you used the homebrew install method,
this will resolve the issue. If you're using Mac OS X, want to build manually
instead of using homebrew, and you get a build failure related to missing
openssl headers, try the following:

```console
$ brew install openssl
$ ./configure --with-libs=/usr/local/opt/openssl
$ make clean check
$ sudo make install
```

More detail about this workaround can be found via `brew info openssl`.


Support
-------

Having trouble with neo4j-client? Please raise any issues with usage on
[StackOverflow](http://stackoverflow.com/questions/tagged/neo4j-client). If
you've found a bug in the code, please raise an issue on
[GitHub](https://github.com/cleishm/libneo4j-client) and include details of how
to reproduce the bug.


Contributing
------------

Contributions to neo4j-client are encouraged and should be made via pull
requests made to the [GitHub repository](
https://github.com/cleishm/libneo4j-client). Please include test cases where
possible, and use a style and approach consistent with the rest of the library.

It may be worthwhile raising an issue on github for the contribution you
intend to make before developing the code, to allow for discussion and feedback
on the requirements.


License
-------

neo4j-client is licensed under the [Apache License, Version 2.0](
http://www.apache.org/licenses/LICENSE-2.0).

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.  See the License for the
specific language governing permissions and limitations under the License.
