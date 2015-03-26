# Introduction #

This is a brief document how to debug problems regarding sjsms and gross. The first method is to test with `imsimta test -mapping` to see if SJSMS can find grosscheck.so  and call the right routine and contact grossd. The other method is to configure network and SJSMS so that you can test grossd with actual mail flow. The latter way involves setting up an ip alias so that you can test with a client address that ends up greylisted and relevant configuration for sjsms to get it log enough debugging info to the logs.

# Method One #

Test your mapping like this:

```
$ sbin/imsimta test -mapping -debug -table ORIG_MAIL_ACCESS 
sbin/imsimta test -mapping -debug -table ORIG_MAIL_ACCESS 
Input string: TCP|127.0.0.2|25|127.0.0.2||||tcp_local|foo||bar
20:00:29.77: Mapping 5 applied to TCP|127.0.0.2|25|127.0.0.2||||tcp_local|foo||bar
20:00:29.77:   Entry #1 matched, pattern "TCP|*|*|*|*|*|*|tcp_local|*|*|*", template "$[/tmp/gross/lib/grosscheck.so,grosscheck,127.0.0.1,0,1111,$2,$=$8$_,$=$6$_]", match #0.
20:00:29.77:   User routine call: /tmp/gross/lib/grosscheck.so\grosscheck(127.0.0.1,0,1111,127.0.0.2,bar,foo) ->
20:00:29.79:     Returned "$X4.4.3|$NPlease$ try$ again$ later".
20:00:29.79:   New target "4.4.3|Please try again later"
20:00:29.79:   Exiting...
20:00:29.79:   Final result "4.4.3|Please try again later"
Output string: 4.4.3|Please try again later
Output flags: [0, 'N' (78), 'X' (88)]
```

There you can see grosscheck returning a `STATUS_GREY` response.

# Method Two #

## Network configuration ##

Set up an ip alias 127.0.0.2 on your mail server host. The exact way to do it depends on your host operating system. Here are some examples:

| OS  | command |
|:----|:--------|
| Solaris | `ifconfig lo0 addif 127.0.0.2 up` |


## SJSMS configuration ##

You have to edit `imta.cnf` and `option.dat` files to enable the debug logging you need. And,
you must compile and enable the new configuration.

### imta.cnf ###

Add `slave_debug` keyword to the `tcp_local` channel.

### option.dat ###

Set `MM_DEBUG=3` in `option.dat` file.

### Enabling the new configuration ###

Run `imsimta cnbuild` to compile the new configuration, and `imsimta restart dispatcher` to make dispatcher aware of the new configuration and the new ip address.

## The log files ##

Done that, SJSMS now makes a new tcp\_local\_slave.log file after each smtp connection ending up on tcp\_local channel.

### Three excerpts from tcp\_local\_slave.log files ###

**Example #1 - Gross not running**

As you can see, grosscheck returns $Y after two seconds.

```
08:56:39.33: Mapping 5 applied to TCP|127.0.0.2|25|127.0.0.2|48088|SMTP/|MAIL|tc
p_local|eino@utu.fi|l|eino3@utu.fi
08:56:39.33:   Entry #1 matched, pattern "TCP|*|*|*|*|*|*|tcp_local|*|*|*", temp
late "$[/tmp/gross/lib/grosscheck.so,grosscheck,127.0.0.1,0,1111,$2,$=$8$_,$=$6$
_]", match #0.
08:56:39.33:   User routine call: /tmp/gross/lib/grosscheck.so\grosscheck(127.0.
0.1,0,1111,127.0.0.2,eino@utu.fi,eino@utu.fi) ->
08:56:41.34:     Returned "$Y".
```

**Example #2 - Gross running: first contact**

This is a first contact: gross tells imta to send a temporary error.

```
09:05:02.87: Mapping 5 applied to TCP|127.0.0.2|25|127.0.0.2|48669|SMTP/|MAIL|tc
p_local|eino@utu.fi|l|eino@utu.fi
09:05:02.87:   Entry #1 matched, pattern "TCP|*|*|*|*|*|*|tcp_local|*|*|*", temp
late "$[/tmp/gross/lib/grosscheck.so,grosscheck,127.0.0.1,0,1111,$2,$=$8$_,$=$6$
_]", match #0.
09:05:02.88:   User routine call: /tmp/gross/lib/grosscheck.so\grosscheck(127.0.
0.1,0,1111,127.0.0.2,eino@utu.fi,eino@utu.fi) ->
09:05:02.91:     Returned "$X4.4.3|$NPlease$ try$ again$ later".
```

**Example #3 - Gross running: second attempt**

Now the same client tries to send message again. Gross shows green light.

```
09:07:37.37: Mapping 5 applied to TCP|127.0.0.2|25|127.0.0.2|48841|SMTP/|MAIL|tc
p_local|eino@utu.fi|l|eino@utu.fi
09:07:37.37:   Entry #1 matched, pattern "TCP|*|*|*|*|*|*|tcp_local|*|*|*", temp
late "$[/tmp/gross/lib/grosscheck.so,grosscheck,127.0.0.1,0,1111,$2,$=$8$_,$=$6$
_]", match #0.
09:07:37.37:   User routine call: /tmp/gross/lib/grosscheck.so\grosscheck(127.0.
0.1,0,1111,127.0.0.2,eino@utu.fi,eino@utu.fi) ->
09:07:37.37:     Returned "$Y".
```







