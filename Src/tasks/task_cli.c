#include "user_tasks.h"

#include <stdio.h>
#include <string.h>

#include "cli.h"
#include "retarget.h"

// ----- TASK PROPERTIES -----
static TaskHandle_t sTH; // task handle
static uint8_t sPrio = 1; // priority
static uint16_t sStkSize = 3072; // stack size
void task_cli(void *pParam); // taszk routine function
// ---------------------------

// structure for defining cli commands
struct CliCommand {
    CliToken_Type ppTok[MAX_TOKEN_N]; // tokens
    uint8_t tokCnt; // number of tokens in command without arguments
    uint8_t minArgCnt; // minimal number of parameters
    char pHelp[MAX_HELP_LEN]; // help line
    char *pHint; // pointer to the hint part of the command (allocated on pHelp)
    uint8_t cmdLen; // length of the command string
    char *pPadding; // padding between command and hint parts (allocated on pHelp)
    fnCliCallback pCB; // processing callback function
};

#define CLI_MAX_CMD_CNT (32) // limit on number of separate commands
static struct CliCommand spCliCmds[CLI_MAX_CMD_CNT];
static uint8_t sCliCmdCnt;
static bool sCmdsTidy = false;

// ---------------------------

// register and initialize task
void reg_task_cli() {
    BaseType_t result = xTaskCreate(task_cli, "cli", sStkSize, NULL, sPrio,
            &sTH);
    if (result != pdPASS) { // taszk létrehozása
        MSG("Failed to create task! (errcode: %ld)\n", result);
    }

    // ----------------------
    sCliCmdCnt = 0;
}

// remove task
void unreg_task_cli() {
    vTaskDelete(sTH); // taszk törlése
}

// ---------------------------

#define CLI_BUF_LENGTH (192)
#define TOK_ARR_LEN (16)

static void tokenize_cli_line(char *pLine, CliToken_Type ppTok[],
        uint32_t tokMaxLen, uint32_t tokMaxCnt, uint32_t *pTokCnt) {
    uint32_t len = strlen(pLine);

    // copy to prevent modifying original one
    static char pLineCpy[CLI_BUF_LENGTH];
    strcpy(pLineCpy, pLine);

    *pTokCnt = 0;

    // prevent processing if input is empty
    if (len == 0 || tokMaxCnt == 0) {
        return;
    }

    // first token
    char *pTok = strtok(pLineCpy, " ");
    strncpy(&ppTok[0][0], pTok, tokMaxLen);
    (*pTokCnt)++;

    // further tokens
    while ((*pTokCnt < tokMaxCnt) && (pTok != NULL)) {
        pTok = strtok(NULL, " ");

        if (pTok != NULL) {
            strncpy(&ppTok[*pTokCnt][0], pTok, tokMaxLen); // store token
            (*pTokCnt)++; // increment processed token count
        }
    }

}

#define HINT_DELIMITER ('\t')
#define MIN_GAP_IN_SPACES (3)

static void tidy_format_commands() {
    if (sCmdsTidy) {
        return;
    }

    // get maximal line length
    uint32_t i, max_line_length = 0;
    for (i = 0; i < sCliCmdCnt; i++) {
        // fetch command descriptor
        struct CliCommand *pCmd = &spCliCmds[i];

        // if this command is unprocessed
        if (pCmd->pHint == NULL) {
            // search for hint text
            char *pDelim = strchr(pCmd->pHelp, HINT_DELIMITER);

            if (pDelim == NULL) {
                pCmd->cmdLen = strlen(pCmd->pHelp); // the whole line only contains the command, no hint
                pCmd->pHint = pCmd->pHelp + pCmd->cmdLen; // make the hint point to the terminating zero
            } else {
                // calculate the length of the command part
                pCmd->cmdLen = pDelim - pCmd->pHelp;

                // split help-line
                *pDelim = '\0'; // get command part
                pCmd->pHint = pDelim + 1; // get hint part

                // trim hint
                while (*(pCmd->pHint) <= ' ') {
                    pCmd->pHint++;
                }

                // allocate padding
                pCmd->pPadding = pCmd->pHint + strlen(pCmd->pHint) + 1;
            }
        }

        // update max line length
        max_line_length = MAX(max_line_length, pCmd->cmdLen);
    }

    // fill-in paddings
    for (i = 0; i < sCliCmdCnt; i++) {
        // fetch command descriptor
        struct CliCommand *pCmd = &spCliCmds[i];

        // calculate padding length
        uint8_t padLen = max_line_length - pCmd->cmdLen + MIN_GAP_IN_SPACES;

        //MSG("pHelp:    %x\npHint:    %d\npPadding: %d\n\n", pCmd->pHelp, pCmd->pHint - pCmd->pHelp, pCmd->pPadding - pCmd->pHint);

        // fill-in padding
        for (uint32_t k = 0; k < padLen; k++) {
            pCmd->pPadding[k] = ' ';
        }

        // terminating zero
        pCmd->pPadding[padLen] = '\0';
    }

    sCmdsTidy = true;
}

void process_cli_line(char *pLine) {
    CliToken_Type ppTok[TOK_ARR_LEN];
    uint32_t tokCnt = 0;

    // tokenize line received from user input
    tokenize_cli_line(pLine, ppTok, MAX_TOK_LEN, TOK_ARR_LEN, &tokCnt);

    if (tokCnt == 0) {
        return;
    }

    int ret = -1;

    // print help
    if (!strcmp(ppTok[0], "?") || !strcmp(ppTok[0], "help")) {
        // tidy-up help if not formatted yet
        tidy_format_commands();

        MSG("\n\n? \t Print this help\n");

        uint8_t i;
        for (i = 0; i < sCliCmdCnt; i++) {
            MSG("%s%s%s\n", spCliCmds[i].pHelp, spCliCmds[i].pPadding,
                    spCliCmds[i].pHint);
        }

        MSG("\n\n");

        ret = 0;
    } else {
        // lookup command
        uint8_t i, k = 0, matchCnt = 0;
        int8_t n = -1;
        for (i = 0; i < sCliCmdCnt; i++) {
            matchCnt = 0;

            for (k = 0; k < spCliCmds[i].tokCnt && k < tokCnt; k++) {
                if (strcmp(ppTok[k], spCliCmds[i].ppTok[k])) {
                    break;
                } else {
                    matchCnt++;
                    if (matchCnt == spCliCmds[i].tokCnt) {
                        n = i;
                        break;
                    }
                }
            }

            if (n != -1) {
                break;
            }
        }

        // call command callback function
        if (n < 0) {
            ret = -1;
        } else {
            struct CliCommand *pCmd = &spCliCmds[n];
            uint8_t argc = tokCnt - pCmd->tokCnt;

            if (argc < pCmd->minArgCnt) {
                MSG("Insufficient parameters, see help! (?)\n");
            } else {
                ret = pCmd->pCB(&ppTok[pCmd->tokCnt], argc);
            }
        }
    }

    if (ret < 0) {
        MSG("Unknown command or bad parameter: '%s', see help! (?)\n", pLine);
    }
}

#define HISTORY_STACK_DEPTH (8)
static char sppCmdHistStk[CLI_BUF_LENGTH][HISTORY_STACK_DEPTH]; // 0: newest element
static size_t sHistStkIdx = 0, sHistStkLevel = 0;

static void put_onto_cmd_hist_stk(const char *pLine) {
    // push elements one slot deeper
    size_t i = 0;
    for (i = 0; (i < sHistStkLevel) && (i < (HISTORY_STACK_DEPTH - 1)); i++) {
        strcpy(sppCmdHistStk[i + 1], sppCmdHistStk[i]);
    }

    // copy the new data
    strcpy(sppCmdHistStk[0], pLine);

    // increase stack level
    sHistStkLevel = MAX(sHistStkLevel + 1, HISTORY_STACK_DEPTH);
}

// special keycodes
#define KC_BS (8)
#define KC_ESC (27)
#define KC_HOME (91)

static void clear_back(size_t n) {
    size_t i = 0;
    // move charet to the beginning of the line
    putchar('\r');

    // fill line with spaces
    for (i = 0; i < n; i++) {
        putchar(' ');
    }

    putchar('\r');
}

static void get_line(char *pStr, size_t *pLen) {
    (*pLen) = 0;

    int c = '\0';
    bool escString = false;

    while (c != '\r' && c != '\n' && (*pLen) < CLI_BUF_LENGTH) {
        vTaskDelay(pdMS_TO_TICKS(10));

        c = getchar();
        putchar(c);

        // ESC received
        if (c == KC_ESC) {
            escString = true;
            continue; // read next char
        }

        if (!escString) { // normal character received
            if (c >= ' ') {
                pStr[(*pLen)++] = c;
            } else if (c == KC_BS) {
                if ((*pLen > 0)) {
                    (*pLen)--;
                    putchar(' ');
                    putchar(KC_BS);
                }
            } else if (c == '\t') {

            }
        } else { // if escape string has been received
            switch (c) { // TODO
            default:
                break;
            }
        }

        // turn off flag
        escString = false;
    }

    // put line onto history stack

    pStr[*pLen] = '\0';
}

static char pBuf[CLI_BUF_LENGTH + 1];

// task routine function
void task_cli(void *pParam) {
    size_t len;

    MSG("CLI on!\n");

    while (1) {
        get_line(pBuf, &len);
        vTaskDelay(pdMS_TO_TICKS(10));

        // replace stream output function
        StreamOutputFunction oldSof = RetargetGetOutput();
        RetargetSetOutput(output_usart);

        process_cli_line(pBuf);

        // restore the old one
        RetargetSetOutput(oldSof);
    }
}

int cli_register_command(char *pCmdParsHelp, uint8_t cmdTokCnt,
        uint8_t minArgCnt, fnCliCallback pCB) {
    // if command storage is full, then return -1;
    if (sCliCmdCnt == CLI_MAX_CMD_CNT) {
    	return -1;
    }

    // obtain pointer to first unused command space
    struct CliCommand *pCmd = &spCliCmds[sCliCmdCnt];

    // tokenize the first part of the line (run until cmkTokCnt tokens have been fetched)
    uint32_t tokCnt = 0;
    tokenize_cli_line(pCmdParsHelp, pCmd->ppTok, MAX_TOK_LEN,
            (cmdTokCnt > TOK_ARR_LEN ? TOK_ARR_LEN : cmdTokCnt), &tokCnt);
    pCmd->tokCnt = (uint8_t) tokCnt;

    // store minimal argument count parameter
    pCmd->minArgCnt = minArgCnt;

    // copy help line
    strncpy(pCmd->pHelp, pCmdParsHelp, MAX_HELP_LEN);

    // zero out hint part (tidy() will fill it)
    pCmd->pHint = NULL;

    // store callback function pointer
    pCmd->pCB = pCB;

    // increase the amount of commands stored
    sCliCmdCnt++;

    // clean up if the same command registered before
    uint8_t i, t;
    int duplicate_idx = -1;
    for (i = 0; i < (sCliCmdCnt - 1); i++) {
        if (spCliCmds[i].tokCnt == cmdTokCnt) {
            for (t = 0; t < cmdTokCnt; t++) {
                if (strcmp(spCliCmds[i].ppTok[t], pCmd->ppTok[t])) {
                    break;
                }
            }

            if (t == cmdTokCnt) {
                duplicate_idx = i;
                break;
            }
        }
    }

    if (duplicate_idx > -1) {
        cli_remove_command(duplicate_idx);
    }

    sCmdsTidy = false; // commands are untidy

    return sCliCmdCnt - 1;
}

void cli_remove_command(int cmdIdx) {
    if (cmdIdx + 1 > sCliCmdCnt || cmdIdx < 0) {
        return;
    }

    uint8_t i;
    for (i = cmdIdx; i < sCliCmdCnt - 1; i++) {
        memcpy(&spCliCmds[i], &spCliCmds[i + 1], sizeof(struct CliCommand));
    }

    sCliCmdCnt--;
}

// removes bunch of commands, terminated by -1
void cli_remove_command_array(int * pCmdHandle) {
	int * pIter = pCmdHandle;
	while (*pIter != -1) {
		cli_remove_command(*pIter);
		pIter++;
	}
}

// -----------------------

bool get_param_value(const CliToken_Type *ppArgs, uint8_t argc,
        const char *pKey, char *pVal) {
    size_t i;
    for (i = 0; i < argc; i++) {
        if (!strncmp(ppArgs[i], pKey, strlen(pKey))) {
            strcpy(pVal, ppArgs[i] + strlen(pKey));
            return true;
        }
    }
    return false;
}
