/*
 * Copyright 2016 Joseph Landry All Rights Reserved
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "pkzip_crypto.h"
#include "crc32.h"

void die(char *reason){
	fflush(stdout);
	fputs(reason, stderr);
	fputs("\n", stderr);
	exit(1);
}

void load_file(char *filename, uint8_t **filedata, long *filesize){
	FILE *f;
	long size;
	uint8_t *data;

	f = fopen(filename, "r");
	if(f != NULL){
		fseek(f, 0, SEEK_END);
		size = ftell(f);
		fseek(f, 0, SEEK_SET);

		if(0 < size){
			data = malloc(size);
			if(data){
				if(fread(data, size, 1, f) == 1){
					fclose(f);
					*filedata = data;
					*filesize = size;
				} else {
					die("error reading file");
				}
			} else {
				die("error allocating");
			}
		} else {
			die("error getting file size");
		}
	} else {
		die("error opening file");
	}

	if(memcmp(data, "VimCrypt~", 9) != 0){
		die("input file is not VimCrypt");
	}

	if(memcmp(data, "VimCrypt~01!", 12) != 0){
		die("input file is not VimCrypt type 01 (PKZIP)");
	}
}


int inc_password(char *password, int max_len, int charset){
	int carry;

	carry = 1;

	while(carry && 0 <= max_len){
		if(charset == 0){
			if(*password < 'a'){
				*password = 'a';
				carry = 0;
			} else if(*password < 'z'){
				*password += 1;
				carry = 0;
			} else if(*password == 'z'){
				*password = 'a';
				carry = 1;
			}
		} else if(charset == 1){
			if(*password < 'A'){
				*password = 'A';
				carry = 0;
			} else if(*password < 'Z'){
				*password += 1;
				carry = 0;
			} else if(*password == 'Z'){
				*password = 'A';
				carry = 1;
			}
		} else if(charset == 2){
			if(*password < 'A'){
				*password = 'A';
				carry = 0;
			} else if(*password < 'Z'){
				*password += 1;
				carry = 0;
			} else if(*password == 'Z'){
				*password = 'a';
				carry = 0;
			} else if(*password < 'z'){
				*password += 1;
				carry = 0;
			} else if(*password == 'z'){
				*password = 'A';
				carry = 1;
			}
		} else if(charset == 3){
			if(*password < '0'){
				*password = '0';
				carry = 0;
			} else if(*password < '9'){
				*password += 1;
				carry = 0;
			} else if(*password == '9'){
				*password = 'A';
				carry = 0;
			} else if(*password < 'Z'){
				*password += 1;
				carry = 0;
			} else if(*password == 'Z'){
				*password = 'a';
				carry = 0;
			} else if(*password < 'z'){
				*password += 1;
				carry = 0;
			} else if(*password == 'z'){
				*password = '0';
				carry = 1;
			}
		} else if(charset == 4){
			if(*password < 0x20){
				*password = 0x20;
				carry = 0;
			} else if(*password < 0x7e){
				*password += 1;
				carry = 0;
			} else {
				*password = 0x20;
				carry = 1;
			}
		} else {
			return 0;
		}
		password++;
		max_len -= 1;
	}
	if(max_len < 0){
		return 0;
	} else {
		return 1;
	}
}

int crack(uint8_t *ciphertext, long length, char *crib, int max_len, int charset, char *start_passwd, FILE *dict){
	char password[32] = {0};
	char *newline;
	char *plaintext;
	uint32_t key[3];
	long i;
	int running;

	if(start_passwd){
		strncpy(password, start_passwd, sizeof(password));
	}
	password[sizeof(password)-1] = 0;

	plaintext = malloc(length+1);
	if(plaintext == NULL){
		die("error allocating memory");
	}

	if(dict){
		if(fgets(password, sizeof(password), dict) == NULL){
			die("error reading from dictionary file");
		}
		newline = strchr(password, '\n');
		if(newline){
			*newline = 0;
		}
	}

	while(running){
		init_key(key, password);

		pkzip_decrypt(key, ciphertext, length, (uint8_t *)plaintext);
		plaintext[length] = 0;
		if(crib != NULL){
			if(strstr(plaintext, crib) != NULL){
				printf("Possible password: '%s'\n", password);
				printf("Plaintext: %-32s\n", plaintext);
			}
		} else {
			for(i = 0; i < length; i++){
				if(plaintext[i] < 0x9 || (0xd < plaintext[i] && plaintext[i] < 0x20) || plaintext[i] == 0xc || 0x7e < plaintext[i]){
					break;
				}
			}
			if(i == length){
				printf("Possible password: '%s'\n", password);
				printf("Plaintext: %-32s\n", plaintext);
			}
		}
		if(dict){
			if(fgets(password, sizeof(password), dict) == NULL){
				running = 0;
			}
			newline = strchr(password, '\n');
			if(newline){
				*newline = 0;
			}
		} else {
			running = inc_password(password, max_len, charset);
		}
	}

	return 0;
}

void help(){
	fprintf(stderr, "crackvim: [options] [filename]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t-b nbytes (default: 128)\n");
	fprintf(stderr, "\t-d dict_file\n");
	fprintf(stderr, "\t-p start_password (default: emty string)\n");
	fprintf(stderr, "\t-C charset (default: 0)\n");
	fprintf(stderr, "\t-l max_passwd_len (default: 6)\n");
	fprintf(stderr, "\t-c crib\n");
	fprintf(stderr, "\n");
}

int main(int argc, char *argv[]){
	char *filename;
	char *crib = NULL;
	uint8_t *filedata;
	long filesize;
	int max_len = 6;
	int charset = 0;
	char *start_passwd = NULL;
	char *dict_filename;
	FILE *dict = NULL;
	int nbytes = 128;
	double velocity;

	if(argc < 2){
		help();
		exit(1);
	} else {
		argv++; argc--;
		while(0 < argc){
			if(strcmp(argv[0], "-c") == 0){
				argc--; argv++;
				if(0 < argc){
					crib = argv[0];
					argc--; argv++;
				} else {
					fprintf(stderr, "\t-c [crib]\n\n");
					fprintf(stderr, "Only report plaintexts containing crib.\n");
					fprintf(stderr, "Without a crib, crackvim will report any plaintext that looks like an ascii text file\n\n");
					exit(1);
				}
			} else if(strcmp(argv[0], "-l") == 0){
				argc--; argv++;
				if(0 < argc){
					max_len = atoi(argv[0]);
					argc--; argv++;
				} else {
					fprintf(stderr, "\t-l [max_passwd_len]\n\n");
					fprintf(stderr, "Only test password up to length.  Default: 6\n\n");
					exit(1);
				}
			} else if(strcmp(argv[0], "-C") == 0){
				argc--; argv++;
				if(0 < argc){
					charset = atoi(argv[0]);
					if(charset < 0 || 3 < charset){
						fprintf(stderr, "Invalid character set\n");
						exit(1);
					}
					argc--; argv++;
				} else {
					fprintf(stderr, "\t-c [charset]\n\n");
					fprintf(stderr, "Character set for password generation\n");
					fprintf(stderr, "\t0: lower alpha (Default)\n");
					fprintf(stderr, "\t1: upper alpha\n");
					fprintf(stderr, "\t2: alpha\n");
					fprintf(stderr, "\t3: alphanum\n");
					fprintf(stderr, "\t4: ascii 0x20 - 0x7e\n");
					fprintf(stderr, "\n");
					exit(1);
				}
			} else if(strcmp(argv[0], "-p") == 0){
				argc--; argv++;
				if(0 < argc){
					start_passwd = argv[0];
					argc--; argv++;
				} else {
					fprintf(stderr, "\t-p [start_password]\n\n");
					fprintf(stderr, "Choose a password to start with.\n");
					fprintf(stderr, "Default value is the empty string.\n");
					fprintf(stderr, "This feature can be used to 'resume' an attack at a specific point\n");
					fprintf(stderr, "\n");
					exit(1);
				}
			} else if(strcmp(argv[0], "-d") == 0){
				argc--; argv++;
				if(0 < argc){
					dict_filename = argv[0];
					argc--; argv++;
					if(strcmp(dict_filename, "-") == 0){
						dict = stdin;
					} else {
						dict = fopen(dict_filename, "r");
						if(dict == NULL){
							die("error opening dictionary file");
						}
					}
				} else {
					fprintf(stderr, "\t-d [dictionary file]\n\n");
					fprintf(stderr, "Dictionary attack.  Use \"-d -\" for stdin\n\n");
					exit(1);
				}
			} else if(strcmp(argv[0], "-b") == 0){
				argc--; argv++;
				if(0 < argc){
					nbytes = atoi(argv[0]);
					argc--; argv++;
					if(nbytes < 0){
						die("nbytes must be positive integer or zero");
					}
				} else {
					fprintf(stderr, "\t-b [nbytes]\n\n");
					fprintf(stderr, "only analyze first nbytes of file.\n");
					fprintf(stderr, "the value 0 means analyze whole file\n");
					exit(1);
				}
			} else {
				break;
			}
		}
		if(0 < argc){
			filename = argv[0];
		} else {
			printf("filename missing\n");
			exit(1);
		}
	}

	load_file(filename, &filedata, &filesize);
	printf("loaded %s: %ld bytes\n", filename, filesize);
	if(crib){
		printf("searching for crib: \"%s\"\n", crib);
	} else {
		printf("searching for ascii text files\n");
	}
	if(dict){
		printf("using dictionary file: %s\n", dict_filename);
	} else {
		printf("using brute force\n");
		printf("max password length: %d\n", max_len);
		printf("charset: %d\n", charset);
		if(start_passwd){
			printf("starting with password: %s\n", start_passwd);
		}
	}
	printf("\n");
	make_crc_table();

	if(nbytes == 0 || nbytes > filesize - 12){
		nbytes = filesize - 12;
	}
	crack(filedata+12, nbytes, crib, max_len, charset, start_passwd, dict);
	return 0;
}
