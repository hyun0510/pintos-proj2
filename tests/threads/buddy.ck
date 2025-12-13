# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(buddy) begin
(buddy) 3 pages allocation
(buddy) PASS: 4pages alloction success
(buddy) --------merge test--------
(buddy) two 16pages allcation -> A, B
(buddy) Allocated A at 320, B at 336
(buddy) free A,B
(buddy) 32pages allocation
(buddy) PASS: Merge success C starts at 320 -> same as previous A/B
(buddy) end
EOF
pass;
