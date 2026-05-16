# Security Constraint:
- **Network Facing:** The application accepts traffic from untrusted networks. Assume all incoming data is hostile.
- **Buffer Safety:** Never trust packet lengths or sizes from the wire. Always validate bounds before `memcpy` or buffer reads/writes. Use safe string functions (`strncpy`, `snprintf`) instead of C-family defaults (`strcpy`, `sprintf`). Use functions of the mem* family in the first place (e.g. `memchr` instead of `strchr`) when the length is known or can be determined. Be mindful of zero-byte injection in external string data.
- **DoS Protection:** Implement rate limiting per IP address/connection.Be mindful of both high-rate floods and low-rate persistent attacks (e.g., Slowloris). A malicious actor should not be able to exhaust the server's resources via connection flooding, state explosion, or resource exhaustion.
- **Integer Safety:** Check for integer overflows before allocation or array indexing. Malformed packets could cause heap corruption or DoS.
- **Endianess:** Be mindfull that protocol data endianess and host cpu endianess might not be the same.
- **Safety-first:** Do not compromise security for performance. Memory safety and validation are paramount.
