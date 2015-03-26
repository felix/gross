# Configuration options #

## Please note, this file does not contain all the options ##

**check http://code.google.com/p/gross/source/browse/trunk/doc/examples/grossd.conf for the complete list**

  * `dnsbl` option is the address of dns blacklist server. There can be multiple `dnsbl` options in the configuration file and each one will be queried in parallel. As soon as one of the blacklist servers recognizes the senders address, the mail will be greylisted. If none of the blacklist servers contain the senders address, then the sender is **trusted**. These values are unused if _grossd_ is configured with `--disable-dnsbl` flag.
  * `host` option is the address of the interface _grossd_ binds itself, default is `localhost`
  * `port` is the port number _grossd_ binds itself, default is `5525`
  * `stat_interval` is the number of _seconds_ between statistics logging
  * `stat_level` is the name of the requested statistic. There can be multiple `stat_level` options in the configuration file (Using both `none` and `full` is undefined). Default is `none`. Valid options are currently:
    * `full` = _grossd_ sends all possible statistics
    * `none` = _grossd_ will not send any statistics
    * `status` = _grossd_ will send basic statistics from the `stat_interval` period
    * `since_startup` = _grossd_ will send basic statistics since the server startup
    * `delay` = _grossd_ will send statistics about query delays
  * `filter_bits` option determines the size of the bloom filter. The size will be 2^filter\_bits, default is `22`.
  * `number_buffers` is the number of bloom filters used in the ring queue, default is `8`.
  * `rotate_interval` is the number of _seconds_ between bloom filter rotation. An individual entry will be removed from the filter after `rotate_interval` times the `number_buffers` _seconds_ at the earliest. Default is `3600`
  * `sync_peer` is the address of the peer used in clustered mode
  * `sync_listen` is the address of the interface to bind to listen for peer communication, default is the value of `host`.
  * `sync_port` is the port number to listen to and connect to in peer communication, default is `5524`
  * `statefile` is path to file that is used to store the state (filters etc.) of the _grossd_ instance
  * `log_method` `syslog` is currently the only method implemented, it is the default.
  * `log_level` default is `info`, the other valid options are `debug`, `notice`, `error` and `warning`
  * `syslog_facility` is the facility _syslog_ sends log messages with.
  * `update` is the way _grossd_ updates its filters. If `update = grey`, filters are only updated if response is `STATUS_GREY`. `update = always` makes _grossd_ to update filters on every query. Default is `grey`.
  * `grey_delay` is the number of _seconds_ between accepting the second sending attempt after greylisting. The default value is 10, increasing the delay causes the amortized memory requirement to grow linearly.
  * `grey_mask` is the mask for grossd to use when matching client\_ip against the database. You may set it to 24 to treat  addresses like a.b.c.d as a.b.c.0.