# Introduction #

Version svn-236 includes a new feature that allows you to configure weights on different checks. You can also configure thresholds for greylisting and blocking independently.

# Details #

An excerpt from grossd.conf:

```
check = dnsbl
check = blocker
blocker_weight = 8
dnsbl=bl.spamcop.net;3
dnsbl=dnsbl.njabl.org;3
dnsbl=dnsbl.sorbs.net;3
dnsbl=dnsbl-1.uceprotect.net;3
dnsbl=dnsbl-2.uceprotect.net;1
dnsbl=dnsbl-3.uceprotect.net;1
dnsbl=rbl-plus.mail-abuse.org;6
dnsbl=zen.spamhaus.org;6
block_threshold = 9
```

If grossd finds that a client hosts ip matches e.g. both zen.spamhaus.org, weight 6, and bl.spamcop.net, weight 3, the total weight from the cheks is 9. That means grossd will block the connection with a permanent error, because total weight is equal to or greater than block\_threshold.

See a complete [configuration file](http://code.google.com/p/gross/source/browse/trunk/doc/examples/grossd.conf) with details.

# Effect #

With the configuration options show above, grossd statistics at the University of Turku look like this:

  * trust: 8.8 %
    * these are from hosts that pass all the checks
  * match: 0.1 %
    * these have been retried after getting greylist treatment
  * greylist: 8.3 %
  * block: 82.7 %

Remember, these are recipients, not hosts. That means that grossd alone blocks over 90 % of mail, that is well over 95 % of spam with the shown configuration.