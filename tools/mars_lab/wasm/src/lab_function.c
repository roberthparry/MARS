/**
 * @file lab_function.c
 * @brief Function-card source selection and programme execution presentation.
 *
 * Projects native programme output and request diagnostics without inspecting
 * mathematical syntax. Scoped browser values preserve source precision, output
 * indentation and diagnostic getter ordering. The host remains responsible for
 * request cancellation, asynchronous execution, ownership checks and final cleanup.
 */
#include "lab_dom.h"
#include "lab_function.h"

/* Select only an enabled Function card's full source, distinguishing empty text from an ineligible card. */
void lab_function_source(void)
{
    int button = lab_dom_id("functionRun"), title = lab_dom_id("functionTitle");
    if (lab_dom_truth(lab_dom_get(button, "disabled")) ||
        !lab_dom_equal(lab_dom_read(title, 0, ""), lab_dom_string("Function"))) {
        lab_dom_return(0);
        return;
    }
    lab_dom_return(lab_dom_clean(lab_dom_read(lab_dom_id("functionStyle"), 3, "fullText"), 0));
}

/* Reset execution presentation after the host has cancelled the previous request. */
void lab_function_clear(void)
{
    lab_dom_disabled(lab_dom_id("functionRun"), 0);
    lab_dom_write(lab_dom_id("functionRunOutput"), 0, "", lab_dom_string(""));
    lab_dom_class(lab_dom_id("functionRunResult"), "hidden", 1);
}

/* Show availability or progress without claiming ownership of the host's asynchronous request. */
void lab_function_start(int available)
{
    lab_dom_class(lab_dom_id("functionRunResult"), "hidden", 0);
    if (available)
        lab_dom_disabled(lab_dom_id("functionRun"), 1);
    lab_dom_write(
        lab_dom_id("functionRunOutput"), 0, "",
        lab_dom_string(available ? "Running…" : "No Function programme is available. Evaluate an input first."));
}

/* Retain output indentation and the established response-versus-exception diagnostic precedence. */
void lab_function_complete(unsigned outcome, int timed_out, int data, int error)
{
    if (outcome != 1 && outcome != 2 && outcome != 4)
        return;
    int text;
    if (outcome == 4) {
        text = timed_out ? lab_dom_string("Execution request timed out.")
                         : lab_dom_join(lab_dom_string("Could not run programme: "), lab_dom_get(error, "message"), "");
    } else {
        int output = lab_dom_clean(lab_dom_get(data, "output"), 3);
        int diagnostic = lab_dom_clean(lab_dom_get(data, "error"), 0);
        if (outcome == 2) {
            text = lab_dom_truth(output) ? output : lab_dom_string("Programme completed without output.");
        } else if (timed_out) {
            text = lab_dom_string("Execution request timed out.");
        } else {
            text = lab_dom_truth(diagnostic) ? diagnostic : lab_dom_string("Programme execution failed.");
            if (lab_dom_truth(output))
                text = lab_dom_join(lab_dom_join(output, lab_dom_string("\n\n"), ""), text, "");
        }
    }
    lab_dom_write(lab_dom_id("functionRunOutput"), 0, "", text);
}
