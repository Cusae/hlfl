/* hlfl
 * Copyright (C) 2000-2002 Renaud Deraison
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Reference : http://netfilter.kernelnotes.org/iptables-HOWTO-7.html
 */

#include "includes.h"
#include "hlfl.h"

static FILE *fout;

extern int matched_if;

/*------------------------------------------------------------------
 * Private functions
 *------------------------------------------------------------------*/

static char *
icmp_types(type)
 char *type;
{
 char *ret = malloc(40 + strlen(type));
 memset(ret, 0, 40 + strlen(type));
 if (!strlen(type))
  return ret;

 if (!strcmp(type, "echo-reply") ||
     !strcmp(type, "destination-unreachable") ||
     !strcmp(type, "echo-request") || !strcmp(type, "time-exceeded") ||
     !strcmp(type, "source-quench") || !strcmp(type, "parameter-problem"))
  sprintf(ret, "--icmp-type %s", type);
 else
  fprintf(stderr, "Warning. Unknown icmp type '%s'\n", type);
 return ret;
}

static char *
netfilter_sports(ports)
 char *ports;
{
 if (!ports || !strlen(ports))
  return strdup("");
 else
   {
    char *ret = malloc(20 + strlen(ports));
    sprintf(ret, "--source-port %s", ports);
    return ret;
   }
}

static char *
netfilter_dports(ports)
 char *ports;
{
 if (!ports || !strlen(ports))
  return strdup("");
 else
   {
    char *ret = malloc(20 + strlen(ports));
    sprintf(ret, "--destination-port %s", ports);
    return ret;
   }
}

/*------------------------------------------------------------------
 * Linux netfilter
 *------------------------------------------------------------------*/
int
translate_linux_netfilter(op, proto, src, log, dst, sports, dports, interface)
 int op;
 char *proto;
 char *src;
 int log;
 char *dst;
 char *sports;
 char *dports;
 char *interface;
{
 char *via_in = strdup("");
 char *via_out = strdup("");
 char *t;
 char *sports_as_src = NULL;
 char *sports_as_dst = NULL;
 char *dports_as_src = NULL;
 char *dports_as_dst = NULL;
 char *icmp_code = NULL;
 char *logit = "";

/* For the moment I leave this to avoid breaking the rules, but it 
should be deleted */
if (log)
  logit = "LOG_";

 if (icmp(proto))
   {
    if (sports && strlen(sports))
     icmp_code = icmp_types(sports);
    else if (dports && strlen(dports))
     icmp_code = icmp_types(dports);
    else
     icmp_code = icmp_types("");

    dports_as_src = dports_as_dst = icmp_code;
    sports_as_src = sports_as_dst = "";
   }
 else
   {
    while ((t = strchr(sports, '-')))
     t[0] = ':';
    while ((t = strchr(dports, '-')))
     t[0] = ':';
    sports_as_src = netfilter_sports(sports);
    sports_as_dst = netfilter_dports(sports);

    dports_as_src = netfilter_sports(dports);
    dports_as_dst = netfilter_dports(dports);
   }

 if (interface)
   {
    free(via_in);
    via_in = malloc(10 + strlen(interface));
    sprintf(via_in, "-i %s", interface);
    via_out = malloc(10 + strlen(interface));
    sprintf(via_out, "-o %s", interface);
   }
 switch (op)
   {
/* 
	This is a really ugly hack... iptables reads the INPUT rules only for 
	packets directed to the system an OUTPUT for packets generated by the
	system. So the rules would work ok. For a "host firewall" but not for 
	a "router firewall". After thinking it for a few days, I decided to 
	implement it the ugly way, mainly duplicating every entry on the 
	FORWARD chain. To assure that it will work under both circumstances. 
	I think the nice way to do it (besides maybe writting some functions 
	to avoid duplication of coding) is to create a new chain and put 
	everything in there and then call it from the three builtin chains 
	in the filter table. But I need to test if this works as expected 
	before coding it here.  

	Carlos
*/
   case ACCEPT_ONE_WAY:
    fprintf(fout, "$iptables -A OUTPUT -s %s -d %s -p %s %s %s -j %sACCEPT %s\n",
	   src, dst, proto, sports_as_src, dports_as_dst, logit, via_out);
    fprintf(fout, "$iptables -A FORWARD -s %s -d %s -p %s %s %s -j %sACCEPT %s\n",
	   src, dst, proto, sports_as_src, dports_as_dst, logit, via_out);
    break;
   case ACCEPT_ONE_WAY_REVERSE:
    fprintf(fout, "$iptables -A INPUT -s %s -d %s -p %s %s %s -j %sACCEPT %s\n", 
	   dst, src, proto, dports_as_src, sports_as_dst, logit, via_in);
    fprintf(fout, "$iptables -A FORWARD -s %s -d %s -p %s %s %s -j %sACCEPT %s\n", 
	   dst, src, proto, dports_as_src, sports_as_dst, logit, via_in);
    break;
   case ACCEPT_TWO_WAYS:
    fprintf(fout, "$iptables -A OUTPUT -s %s -d %s -p %s %s %s -j %sACCEPT %s\n",
	   src, dst, proto, sports_as_src, dports_as_dst, logit, via_out);
    fprintf(fout, "$iptables -A INPUT -s %s -d %s -p %s %s %s -j %sACCEPT %s\n", 
	   dst, src, proto, dports_as_src, sports_as_dst, logit, via_in);
    fprintf(fout, "$iptables -A FORWARD -s %s -d %s -p %s %s %s -j %sACCEPT %s\n",
	   src, dst, proto, sports_as_src, dports_as_dst, logit, via_out);
    fprintf(fout, "$iptables -A FORWARD -s %s -d %s -p %s %s %s -j %sACCEPT %s\n", 
	   dst, src, proto, dports_as_src, sports_as_dst, logit, via_in);
    break;
   case ACCEPT_TWO_WAYS_ESTABLISHED:
    fprintf(fout, "$iptables -A OUTPUT -s %s -d %s -p %s %s %s -j %sACCEPT %s\n",
	      src, dst, proto, sports_as_src, dports_as_dst, logit, via_out);
    fprintf(fout, "$iptables -A INPUT -s %s -d %s -p %s %s %s -m state\
		       --state ESTABLISHED -j %sACCEPT %s\n",
	      dst, src, proto, dports_as_src, sports_as_dst, logit, via_in);
    fprintf(fout, "$iptables -A FORWARD -s %s -d %s -p %s %s %s -j %sACCEPT %s\n",
	      src, dst, proto, sports_as_src, dports_as_dst, logit, via_out);
    fprintf(fout, "$iptables -A FORWARD -s %s -d %s -p %s %s %s -m state\
		       --state ESTABLISHED -j %sACCEPT %s\n",
	      dst, src, proto, dports_as_src, sports_as_dst, logit, via_in);
    break;
   case ACCEPT_TWO_WAYS_ESTABLISHED_REVERSE:
    fprintf(fout, "$iptables -A INPUT -s %s -d %s -p %s %s %s -j %sACCEPT %s\n",
	   dst, src, proto, dports_as_src, sports_as_dst, logit, via_in);
    fprintf(fout, "$iptables -A OUTPUT -s %s -d %s -p %s %s %s -m state\
		       --state ESTABLISHED -j %sACCEPT %s\n",
	      src, dst, proto, sports_as_src, dports_as_dst, logit, via_out);
    fprintf(fout, "$iptables -A FORWARD -s %s -d %s -p %s %s %s -j %sACCEPT %s\n",
	   dst, src, proto, dports_as_src, sports_as_dst, logit, via_in);
    fprintf(fout, "$iptables -A FORWARD -s %s -d %s -p %s %s %s -m state\
		       --state ESTABLISHED -j %sACCEPT %s\n",
	      src, dst, proto, sports_as_src, dports_as_dst, logit, via_out);
    break;
   case DENY_ALL:
    fprintf(fout, "$iptables -A OUTPUT -s %s -d %s -p %s %s %s -j %sDROP %s\n", 
	   src, dst, proto, sports_as_src, dports_as_dst, logit, via_out);
    fprintf(fout, "$iptables -A INPUT -s %s -d %s -p %s %s %s -j %sDROP %s\n", 
	   dst, src, proto, dports_as_src, sports_as_dst, logit, via_in);
    fprintf(fout, "$iptables -A FORWARD -s %s -d %s -p %s %s %s -j %sDROP %s\n", 
	   src, dst, proto, sports_as_src, dports_as_dst, logit, via_out);
    fprintf(fout, "$iptables -A FORWARD -s %s -d %s -p %s %s %s -j %sDROP %s\n", 
	   dst, src, proto, dports_as_src, sports_as_dst, logit, via_in);
    break;
   case REJECT_ALL:
    /* Missing some stuff here to really reject with rst, but needs to be
     * done according to protocol, should be added to tcp 
     * if (!strcmp(proto, "tcp")....and all 
     *
     * Carlos
     *
     * */
    fprintf(fout, "$iptables -A OUTPUT -s %s -d %s -p %s %s %s -j %sREJECT %s\n",
	   src, dst, proto, sports_as_src, dports_as_dst, logit, via_out);
    fprintf(fout, "$iptables -A INPUT -s %s  -d %s -p %s %s %s -j %sREJECT %s\n",
	   dst, src, proto, dports_as_src, sports_as_dst, logit, via_in);
    fprintf(fout, "$iptables -A FORWARD -s %s -d %s -p %s %s %s -j %sREJECT %s\n",
	   src, dst, proto, sports_as_src, dports_as_dst, logit, via_out);
    fprintf(fout, "$iptables -A FORWARD -s %s  -d %s -p %s %s %s -j %sREJECT %s\n",
	   dst, src, proto, dports_as_src, sports_as_dst, logit, via_in);
    break;
   case DENY_OUTPUT:
    fprintf(fout, "$iptables -A OUTPUT -s %s -d %s -p %s %s %s -j %sDROP %s\n",
	   src, dst, proto, sports_as_src, dports_as_dst, logit, via_out);
    fprintf(fout, "$iptables -A FORWARD -s %s -d %s -p %s %s %s -j %sDROP %s\n",
	   src, dst, proto, sports_as_src, dports_as_dst, logit, via_out);
    break;
   case DENY_INPUT:
    fprintf(fout, "$iptables -A INPUT -s %s -d %s -p %s %s %s -j %sDROP %s\n", 
	   dst, src, proto, dports_as_src, sports_as_dst, logit, via_in);
    fprintf(fout, "$iptables -A FORWARD -s %s -d %s -p %s %s %s -j %sDROP %s\n", 
	   dst, src, proto, dports_as_src, sports_as_dst, logit, via_in);
    break;
   case REJECT_OUTPUT:
    fprintf(fout, "$iptables -A OUTPUT -s %s -d %s -p %s %s %s -j %sREJECT %s\n",
	   src, dst, proto, sports_as_src, dports_as_dst, logit, via_out);
    fprintf(fout, "$iptables -A FORWARD -s %s -d %s -p %s %s %s -j %sREJECT %s\n",
	   src, dst, proto, sports_as_src, dports_as_dst, logit, via_out);
    break;
   case REJECT_INPUT:
    fprintf(fout, "$iptables -A INPUT -s %s -d %s -p %s %s %s -j %sREJECT %s\n", 
	   dst, src, proto, dports_as_src, sports_as_dst, logit, via_in);
    fprintf(fout, "$iptables -A FORWARD -s %s -d %s -p %s %s %s -j %sREJECT %s\n", 
	   dst, src, proto, dports_as_src, sports_as_dst, logit, via_in);
    break;
   }
 if (icmp(proto))
   {
    free(icmp_code);
   }
 else
   {
    free(sports_as_src);
    free(sports_as_dst);
    free(dports_as_src);
    free(dports_as_dst);
   }
 free(via_in);
 free(via_out);
 return 0;
}

int
translate_linux_netfilter_start(FILE *output_file)
{
 fout = output_file;

 fprintf(fout, "#!/bin/sh\n");
 fprintf(fout, "# Firewall rules generated by hlfl\n\n");

 fprintf(fout, "iptables=\"/sbin/iptables\"\n\n");
 fprintf(fout, "$iptables -F\n");
 fprintf(fout, "$iptables -X\n\n");

 fprintf(fout, "# This is intended to suppor logging, still hasn't been tested.\n");
 fprintf(fout, "$iptables -N LOG_ACCEPT\n");
 fprintf(fout, "$iptables -A LOG_ACCEPT -j LOG --log-level %s --log-prefix %s\n", 
	HLFL_LINUX_netfilter_LOG_LEVEL, HLFL_LINUX_netfilter_LOG_PREFIX); 
 fprintf(fout, "$iptables -A LOG_ACCEPT -j ACCEPT\n"); 
 fprintf(fout, "$iptables -N LOG_DENY\n");
 fprintf(fout, "$iptables -A LOG_DENY -j LOG --log-level %s --log-prefix %s\n", 
	HLFL_LINUX_netfilter_LOG_LEVEL, HLFL_LINUX_netfilter_LOG_PREFIX); 
 fprintf(fout, "$iptables -A LOG_DENY -j DENY\n"); 
 fprintf(fout, "$iptables -N LOG_REJECT\n");
 fprintf(fout, "$iptables -A LOG_REJECT -j LOG --log-level %s --log-prefix %s\n", 
	HLFL_LINUX_netfilter_LOG_LEVEL, HLFL_LINUX_netfilter_LOG_PREFIX); 
 fprintf(fout, "$iptables -A LOG_REJECT -p tcp -j REJECT --reject-with tcp-reset\n"); 
 fprintf(fout, "$iptables -A LOG_REJECT -j REJECT\n\n"); 

 return 0;
}

void
print_comment_netfilter(buffer)
 char *buffer;
{
 fprintf(fout, buffer);
}

void
include_text_netfilter(c)
 char *c;
{
 if (!strncmp("if(", c, 3))
   {
    if (!strncmp("if(netfilter)", c, strlen("if(netfilter)")))
      {
       fprintf(fout, "%s", c + strlen("if(netfilter)"));
       matched_if = 1;
      }
    else
     matched_if = 0;
   }
 else
  fprintf(fout, "%s", c);
}
