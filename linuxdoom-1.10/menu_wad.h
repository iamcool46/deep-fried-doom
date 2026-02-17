//-----------------------------------------------------------------------------
// WAD Selection Menu - IWAD/PWAD selection before game starts
//-----------------------------------------------------------------------------

#ifndef __MENU_WAD__
#define __MENU_WAD__

#ifdef _WIN32

#ifdef __cplusplus
extern "C" {
#endif

// Run the WAD selection menu. Returns 1 if user chose to run, 0 if quit.
// On return, wadfiles[] array is populated with selected IWAD + PWADs.
int Menu_RunWADSelection(void);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */
#endif /* __MENU_WAD__ */
