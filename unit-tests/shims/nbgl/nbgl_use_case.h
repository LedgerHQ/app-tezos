/* Host stand-in for the Ledger SDK <nbgl_use_case.h>, for unit tests only.

   Just the types and the functions app/src/ui/ui_stream_nbgl.c uses. The
   functions are defined by the test that links against it, so it can model
   what the real ones answer. */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NB_MAX_DISPLAYED_PAIRS_IN_REVIEW 4

typedef void (*nbgl_callback_t)(void);
typedef void (*nbgl_choiceCallback_t)(bool confirm);

typedef struct {
    const char *item;
    const char *value;
} nbgl_layoutTagValue_t;

typedef struct {
    const nbgl_layoutTagValue_t *pairs;
    const nbgl_layoutTagValue_t *(*callback)(uint8_t index);
    uint8_t nbPairs;
    uint8_t startIndex;
    bool    smallCaseForValue;
    bool    wrapping;
} nbgl_layoutTagValueList_t;

typedef nbgl_layoutTagValueList_t nbgl_contentTagValueList_t;

typedef struct {
    int unused;
} nbgl_icon_details_t;

typedef uint8_t nbgl_operationType_t;
#define TYPE_TRANSACTION    1
#define SKIPPABLE_OPERATION 0x80

typedef enum {
    STATUS_TYPE_TRANSACTION_SIGNED,
    STATUS_TYPE_TRANSACTION_REJECTED,
} nbgl_reviewStatusType_t;

uint8_t nbgl_useCaseGetNbTagValuesInPage(
    uint8_t nbPairs, const nbgl_contentTagValueList_t *tagValueList,
    uint8_t startIndex, bool *requireSpecificDisplay);
void nbgl_useCaseReviewStatus(nbgl_reviewStatusType_t reviewStatusType,
                              nbgl_callback_t         quitCallback);
void nbgl_useCaseChoice(const nbgl_icon_details_t *icon, const char *message,
                        const char *subMessage, const char *confirmText,
                        const char           *cancelText,
                        nbgl_choiceCallback_t callback);
void nbgl_useCaseConfirm(const char *message, const char *subMessage,
                         const char *confirmText, const char *cancelText,
                         nbgl_callback_t callback);
void nbgl_useCaseStatus(const char *message, bool isSuccess,
                        nbgl_callback_t quitCallback);
void nbgl_useCaseReviewStart(const nbgl_icon_details_t *icon,
                             const char                *reviewTitle,
                             const char                *reviewSubTitle,
                             const char                *rejectText,
                             nbgl_callback_t            continueCallback,
                             nbgl_callback_t            rejectCallback);
void nbgl_useCaseReviewStreamingStart(nbgl_operationType_t operationType,
                                      const nbgl_icon_details_t *icon,
                                      const char                *reviewTitle,
                                      const char           *reviewSubTitle,
                                      nbgl_choiceCallback_t choiceCallback);
void nbgl_useCaseReviewStreamingContinue(
    const nbgl_contentTagValueList_t *tagValueList,
    nbgl_choiceCallback_t             choiceCallback);
void nbgl_useCaseReviewStreamingContinueExt(
    const nbgl_contentTagValueList_t *tagValueList,
    nbgl_choiceCallback_t choiceCallback, nbgl_callback_t skipCallback);
void nbgl_useCaseReviewStreamingFinish(const char           *finishTitle,
                                       nbgl_choiceCallback_t choiceCallback);
