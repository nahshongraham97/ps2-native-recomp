# Publication wrapper schema correction

The independent saved-review new_executions field is an exact target-name list. The initial publisher compared it to the integer2 and stopped during read-only preflight, before staging or commit. Root corrected the publisher to compare the actual target list to producer executed_clients and separately check its length. Source, assertions, successful CPU results and independent review were unchanged; no tests were rerun. Exact prior publisher bytes are preserved. Future schema inspection must show field types as well as collection lengths.
