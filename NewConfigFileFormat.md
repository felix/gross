# Introduction #

I found a solid candidate for the config file parser library for Gross. Its name is [Confuse](http://www.nongrnu.org/confuse). It's fully featured and seems quite mature and clean. And it has a compatible licence so we can include it in the source tree.

# Config file prototype #

```
# This is subject to chanage

global {
    host = ${HOST:-localhost} # environment variables!
    statefile = /var/db/grossd.state
    pid {
        file = /var/db/grossd.pid
        check = true
    }
    logging syslog {
         level = notice
         facility = mail
    }
    logging file {
        level = info
        path = /var/log/grossd.log
    }
}

protocol sjsms {
    host = localhost # if differs from global option
    port = 1111
}

protocol postfix {}

protocol milter {
    listen = inet:5523@localhost
}

check spamhaus {
    type = dnsbl
    zone = zen.spamhaus.org
    in 127.0.0.4/30 {
        weight = 4
    }
    in 127.0.0.2/32 {
        weight = 8
    }
    weight = 1
}

check spamcop {
    type = dnsbl
    zone = bl.spamcop.net
    weight = 2
    delay = 100 ms
}

check sorbs {
    type = dnsbl
    zone = dnsbl.sorbs.net
   delay = 200 ms
}

check bondedsender {
    type = dnswl
    zone = query.bondedsender.org
}

check blocker {
   type = blocker
   host = localhost
   port = 4466
   weight = 1
}

policy {
    block {
        threshold = 9
        reason = "Bad reputation"
        replycode = 502
    }
    greylist {
        threshold = 1
        delay = 10s
        reason = "Please, try again later"
        replycode = 452
        mask = 24
    }
    timelimit = 5s
}

```