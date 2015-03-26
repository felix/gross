# Introduction #

Gross is a greylisting server. The features that make gross
stand out from other greylisters are:

  * it's blazingly fast
  * it's amazingly resource efficient
  * it can be configured to query DNSBL databases, and enforce greylisting only for hosts that are found on those databases
  * it can block hosts that match multiple DNSBL's
  * it can be replicated and run parallel on two servers
  * It supports Sun Java System Messaging Server, Postfix, Sendmail and Exim.

Currently there is only source distribution available for download. It includes a milter implementation, two new checks and some improvements under the hood. Check the README file in the source package, or ask for more information on the mailing list.

Gross has been a great success in a few sites, including the University of Turku and the University of Wisconsin. Production sites are known to run SJSMS, Postfix, Sendmail and Exim. Please, share your experiences on the mailing list!

# Authors #

Gross is written by Eino Tuominen <eino@utu.fi> and
Antti Siira <antti@utu.fi>




