/**
 * Type declarations for @capacitor-community/barcode-scanner
 * Only loaded at runtime when running in a Capacitor native shell.
 */
declare module '@capacitor-community/barcode-scanner' {
    interface CheckPermissionResult {
        granted: boolean;
        denied: boolean;
        asked: boolean;
        neverAsked: boolean;
    }

    interface ScanResult {
        hasContent: boolean;
        content?: string;
        format?: string;
    }

    export const BarcodeScanner: {
        checkPermission(options?: { force?: boolean }): Promise<CheckPermissionResult>;
        startScan(): Promise<ScanResult>;
        stopScan(): Promise<void>;
        hideBackground(): void;
        showBackground(): void;
        prepare(): Promise<void>;
    };
}
