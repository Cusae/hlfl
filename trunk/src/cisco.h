#ifndef HLFL__CISCO__H
#define HLFL__CISCO_H

int translate_cisco(int, char *, char *, int, char *, char *, char *, char *);
int translate_cisco_start();
void cisco_comment(char *);
void cisco_exit();
void include_text_cisco(char *);
#endif				/* HLFL__CISCO_H */