# stg
Simple TeleGram

> [!WARNING]
> STG is created for my own use => it is not perfect

## Build

Clone the project

``` console
$ git clone --recursive https://github.com/kototuz/stg.git
```
or
``` console
$ git clone --recurse-submodules https://github.com/kototuz/stg.git
```

Go to the project directory and build it

``` console
$ make
```

Run

``` console
$ ./build/stg <your_api_id> <your_api_hash>
```

## Description

At the moment **stg** supports only few operations: login and logout.
You can write messages and they will be displayed in the chat but you can't
send messages yet.

To logout write `:l` in the editor

All customization is located in `src/config.h`
