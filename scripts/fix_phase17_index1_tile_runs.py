from pathlib import Path

path = Path("src/recovered/texture_bridge_match.c")
text = path.read_text()
old = r'''    static const struct {
        uint32_t row;
        uint32_t column;
        const char *text;
    } lines[] = {
        {UINT32_C(5),  UINT32_C(20), "PLAYER      1P       2P"},
        {UINT32_C(8),  UINT32_C(20), "UP      :   ON       ON"},
        {UINT32_C(10), UINT32_C(20), "DOWN    :   ON       ON"},
        {UINT32_C(12), UINT32_C(20), "RIGHT   :   ON       ON"},
        {UINT32_C(14), UINT32_C(20), "LEFT    :   ON       ON"},
        {UINT32_C(18), UINT32_C(20), "PUNCH   :   ON       ON"},
        {UINT32_C(20), UINT32_C(20), "KICK    :   ON       ON"},
        {UINT32_C(22), UINT32_C(20), "GUARD   :   ON       ON"},
        {UINT32_C(26), UINT32_C(20), "START   :   OFF      OFF"},
        {UINT32_C(30), UINT32_C(23), "COIN CHUTE 1 : OFF"},
        {UINT32_C(32), UINT32_C(23), "COIN CHUTE 2 : OFF"},
        {UINT32_C(34), UINT32_C(23), "SERVICE SW   : OFF"},
        {UINT32_C(36), UINT32_C(23), "TEST SW      : OFF"},
        {UINT32_C(45), UINT32_C(20), "PUSH TEST BUTTON TO EXIT"}
    };
'''
new = r'''    static const struct {
        uint32_t row;
        uint32_t column;
        const char *text;
    } lines[] = {
        {UINT32_C(5),  UINT32_C(20), "PLAYER      1P       2P"},
        {UINT32_C(8),  UINT32_C(20), "UP      :"},
        {UINT32_C(8),  UINT32_C(32), "ON "},
        {UINT32_C(8),  UINT32_C(41), "ON "},
        {UINT32_C(10), UINT32_C(20), "DOWN    :"},
        {UINT32_C(10), UINT32_C(32), "ON "},
        {UINT32_C(10), UINT32_C(41), "ON "},
        {UINT32_C(12), UINT32_C(20), "RIGHT   :"},
        {UINT32_C(12), UINT32_C(32), "ON "},
        {UINT32_C(12), UINT32_C(41), "ON "},
        {UINT32_C(14), UINT32_C(20), "LEFT    :"},
        {UINT32_C(14), UINT32_C(32), "ON "},
        {UINT32_C(14), UINT32_C(41), "ON "},
        {UINT32_C(18), UINT32_C(20), "PUNCH   :"},
        {UINT32_C(18), UINT32_C(32), "ON "},
        {UINT32_C(18), UINT32_C(41), "ON "},
        {UINT32_C(20), UINT32_C(20), "KICK    :"},
        {UINT32_C(20), UINT32_C(32), "ON "},
        {UINT32_C(20), UINT32_C(41), "ON "},
        {UINT32_C(22), UINT32_C(20), "GUARD   :"},
        {UINT32_C(22), UINT32_C(32), "ON "},
        {UINT32_C(22), UINT32_C(41), "ON "},
        {UINT32_C(26), UINT32_C(20), "START   :"},
        {UINT32_C(26), UINT32_C(32), "OFF"},
        {UINT32_C(26), UINT32_C(41), "OFF"},
        {UINT32_C(30), UINT32_C(20), "   COIN CHUTE 1 :"},
        {UINT32_C(30), UINT32_C(38), "OFF"},
        {UINT32_C(32), UINT32_C(20), "   COIN CHUTE 2 :"},
        {UINT32_C(32), UINT32_C(38), "OFF"},
        {UINT32_C(34), UINT32_C(20), "   SERVICE SW   :"},
        {UINT32_C(34), UINT32_C(38), "OFF"},
        {UINT32_C(36), UINT32_C(20), "   TEST SW      :"},
        {UINT32_C(36), UINT32_C(38), "OFF"},
        {UINT32_C(45), UINT32_C(20), "PUSH TEST BUTTON TO EXIT"}
    };
'''
if text.count(old) != 1:
    raise SystemExit(f"expected one lines block, found {text.count(old)}")
path.write_text(text.replace(old, new, 1))
