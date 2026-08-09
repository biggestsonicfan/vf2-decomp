/*
 * Compatibility symbol for callers that only need the legacy null backend.
 * The portable framebuffer/input implementation lives in platform_software.c.
 */
void vf2_platform_null_placeholder(void)
{
}
