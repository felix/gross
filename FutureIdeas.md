# Introduction #

This file is for listing all the development ideas for the future. Some of these will get implemented while some will be ever forgotten... Just write on the wall and think about the consequenses later...

## Really Soon, No Kidding ##

**Push out version 1.0!** So this means that before it there will be no new features, only bugfixes and minor modifications. After that I'll once again send a spa<sup>^</sup>H<sup>^</sup>H<sup>^</sup>Hannouncement on various email lists, notably iMS-info, postfix-users and hied-emailadmin.

## Real Soon Now ##

A couple of perfomance tweaks:
  * a thread pool for each dnsbl, this makes it more easy to:
  * implement delays for dnsbl checks so that you can fire up a few most effective lists first to catch the majority of spam without all the dns queries. That way you could use lots of lists without being concerned about load on dns resolver.
  * as c-ares is asynchronous already, make more use of it and make dnsbl check handle multiple requests asynchronously. This leads to fewer idling threads, which is a good thing on some platforms.
  * now that all checks may lead blocking the mail there's a need to wait only those checks that can return a STATUS\_PASS status. It's a little modification, but I don't want to mess the code before 1.0. I wan't to refactor that worker function anyway, it's such a huge blob...
  * run all the checks even if triplet matches, and return match only if triplet passes the tests. It must be configurable, though.

## I'm Gonna Do These ##

Concentrate on Milter possibilities. SJSMS 6.3 has milter support, too. Grossd could (optionally) parse the whole messages finding all the URLs. Spamhaus recommends checking all host IP's and their name server IP's against the SBL.

## Possibly Never ##

Reputation database for gross and others to query. I'd like to build up a reputation database from grossd and PureMessage logs. It could classify hosts/networks/operators based on the historical email volume and the cleanliness of the mail flow. Then I could find if some network that normally sent us like 10 messages a day, started flooding us 100 messages a minute, and force some throttling policy for the flow.

That could even be extended to an early warning system if we just figured out how to merge the data from different organizations.