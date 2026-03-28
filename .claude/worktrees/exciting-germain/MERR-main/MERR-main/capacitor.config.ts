const config = {
    appId: 'nz.harvestpro.app',
    appName: 'HarvestPro NZ',
    webDir: 'dist',
    server: {
        androidScheme: 'https',
    },
    plugins: {
        BarcodeScanner: {},
        Camera: {
            presentationStyle: 'fullscreen',
        },
        SplashScreen: {
            launchAutoHide: true,
            launchShowDuration: 2000,
            backgroundColor: '#0f172a',
            showSpinner: true,
            spinnerColor: '#22c55e',
        },
        StatusBar: {
            style: 'dark',
            backgroundColor: '#0f172a',
        },
    },
};

export default config;
