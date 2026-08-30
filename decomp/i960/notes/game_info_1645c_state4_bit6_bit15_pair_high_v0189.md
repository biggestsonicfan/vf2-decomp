# v0189: state-4 bit6+bit15 pair-high poststate

Entry `0x0001645c`, both fighter state bytes 4. Covers low `0x8040` (bits 6+15) combined with exactly two high bits among 21, 26, 29, 30, and 31.

ROM-backed differential measurement requires subtracting 3 native instructions for unilateral distributions and 6 for bilateral, guarded against underflow. Final compare is always `LESS`. The stale frame two levels above the returned frame restores `r3=0x41000000`, `r4=0x07800f0f`, and `r7=0x41000000`; when fighter 1 participates it also restores `r8=r12=0x07800f0f`, `r13=0x3f6b871d`, and `r15=1`. No mutable Model 2 RAM correction is required.

Verification: all 10 high-bit pairs x 3 fighter distributions x 2 countdown values x 2 mode-bit-6 values x thresholds 0/1/2 pass **360/360 exact ROM-backed snapshots**. Threshold `-1` passes **120/120** as a negative control. Standard CTest gate passes **23/23**.
