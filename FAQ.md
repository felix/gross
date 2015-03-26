### Q: My Question isn't here, what do I do? ###
**Answer**:  Gross is amazingly simple in concept, but it can be daunting to understand what's going on behind the scenes.  Email the [Gross mailing list](https://lists.utu.fi/mailman/listinfo/gross) if you have any questions.

### Q: How do I upgrade Gross ###
**Answer**: It should always be the case that a newer server version should be able to synchronize from an older server, and a newer server should be compatible with an older client.  Check the NEWS file for specifics.

If you have problems upgrading, or if you change the _filter\_bits_ or _number\_buffers_ settings, then you should delete the state file and recreate it.  The side effect of deleting the state file is that the "greylist" triplets will suffer one more round of greylisting.

### Q: Where is the log of changes? ###
**Answer**:  See the NEWS file.

### Q: Should I use a caching DNS server? ###
**Answer**:  Yes.  (more details needed)

### Q: Which DNSBLs should I use? ###
**Answer**:  Here is a list of our favorite DNSBLs:

  * Sophos Blocker - Proprietary component of Sophos PureMessage.  Gross queries the blocker in a custom way.  Risk of false positives: _low_.
  * SpamHaus - A very accurate service.  Free for smaller sites.  Risk of false positives: _low_.
  * SpamCop - (more details needed)
  * Mail-abuse,org - (more details needed)
  * Njabl.org - (more details needed)
  * Sorbs.net - (more details needed)
  * Uceprotect.net - (more details needed)
    * Level 1: Risk of false positives: _medium_
    * Level 2: Risk of false positives: _high_
    * Level 3: Risk of false positives: _high_
  * Abuseat.org - (more details needed)
  * Sorbs.net - (more details needed)

### Q: Can Gross be configured to give perpetual 4xy errors instead of 5xy? ###
**Answer**:  Yes, if the block threshold is met, it means that the bloom filters won't be updated.  If you change the sjsms\_reponse\_block to a 4xy error, it means there will be perpetual temp blocks.

Returning 5xy is more polite in some ways, because senders get notified immediately of a delivery failure. With 4xy it takes at least a couple of hours (4 is typical) for sender to get the notification.

However, it's possible for the owner of a legitimate server that was falsely blacklisted to request delisting and still have those deferred messages deliver.  You might get fewer complaints from users, or the complaints might be different.

### Q: How do I get stats? ###
**Answer**:  Enable stats in the configuration.  You can query gross on port 5522, or look in the log.

### Q: What does the error "maximum thread count (100) reached" mean? ###
**Answer**:  It means you are doing a lot of dns checks, or the checks are taking so long that the thread processing pool fills up.

Setting _block\_threshold_ high will cause gross to wait until block\_threshold is met, or all of the dnsbl queries finish or timeout.  Adjust _block\_threshold_, _pool\_maxthreads_, and _query\_timelimit_ appropriately.  Also, remove any poorly performing dnsbls, or install caching name servers.

If _block\_threshold_ is set to 0, the dnsbl checks will shortcut when the first one matches.

### Q: How do the bloom filters work, and how do I configure fiter\_bits, number\_buffers and rotate\_interval? ###
**Answer**:  (more details needed)

### Q: Why does grey\_mask have to be set at 24? ###
**Answer**:  It doesn't.  UW-Madison uses 32 without any known issues.

### Q: How high can grey\_delay be set? ###
**Answer**:  (more details needed)

### Q: What does the error "Unknown message from client" mean? ###
**Answer**:  It means there was a change in the gross client protocol.  In this case, upgrade the servers before upgrading the client software.

### Q: What does "got: P" mean? ###
**Answer**:  It's normal acknowledgment, and means 'Processing'. Because grosscheck uses udp protocol there's no guarantee that the server will ever receive a request that grosscheck sends. Grosscheck has 2 seconds timeout after which it tries to connect to another grossd node, if configured. Before weighted checks and definitive checks (e.g. dnswl) were introduced, it was a very rare occasion. That's why you have not seen it before.

### Q: Don't you need to escape spaces in the block\_reason? ###
**Answer**:  There's a function that converts reason string accordingly. Postfix and Sendmail and SJSMS all want the string formatted differently. See assemble\_mapresult() and mappingstr() in worker\_sjsms.c.

### Q: Can I compile the client libaries on servers that don't have c-ares libs? ###
**Answer**:  Yes.  Use the --disable-dnsbl option to the configure command.

### Q: I have a question about SJSMS. ###
**Answer**:  See the info-ims@arnold.com mailing list.

### Q: How well does gross scale? ###
**Answer**:  It scales extremely well.  UW-Madison handles over 10 million message attempts per day, with grossd running on solaris 10 sparse zones on two of their four incoming mail servers (running SJSMS).  Gross saved UW-Madison from a 100 million message spam storm in a single day, without any performance problems (no one even noticed until the next day when we were looking at the reports.)

### Q: What does the error "incrementing tolerance counter for dnsbl" mean? ###
**Answer**: Grossd notices if some configured dnsbl causes timeouts. Every timeout decrements a dnsbl specific tolerance counter and grossd ceases to send new queries if that counter hits 0. Grossd increments those counters every 10 seconds and logs the action. This enables grossd to query unresponsive lists only once in every ten seconds.

### Q: Are there risks of database problems? ###
**Answer**:  Gross uses [bloom filters](http://pages.cs.wisc.edu/~cao/papers/summary-cache/node8.html) for its greylist database.  This means that false positives can be likely (see tuning), but false negatives are impossible, and performance is excellent. False positives here mean false database hits which eventually lead false "matches" in greylisting filte, which leads to spam getting through Gross. If you get more than 10 million messages a day you might need to adjust the default values.

### Q: I see "matches" for some IPs but there are no corresponding "greylist" logs? ###
**Answer**:  This means that there are false positives in the bloom filters.  Your mail volumes might be too high, so adjust your filter\_bits and rotate\_interval accordingly.  If you are using weighted checks, then lower block\_theshold, as it will cause fewer updates in the database.

### Q: Why are there so many TIME\_WAIT connections to the Sophos Blocker? ###
**Answer**:  Grossd opens a new TCP connection for each blocker query.  This is a problem with the Blocker (we believe).  Try lowering your tcp idle timeout setting.

### Q: Can I configure both statefile and sync\_peer? ###
**Answer**: Absolutely.

### Q: ? ###
**Answer**: