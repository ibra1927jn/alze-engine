declare module 'html5-qrcode' {
    export class Html5QrcodeScanner {
        constructor(
            elementId: string,
            config: {
                fps: number;
                qrbox?: number | { width: number; height: number };
                aspectRatio?: number;
                disableFlip?: boolean;
                videoConstraints?: MediaTrackConstraints;
                showTorchButtonIfSupported?: boolean;
            },
            verbose?: boolean
        );
        render(
            // eslint-disable-next-line @typescript-eslint/no-explicit-any
            onScanSuccess: (decodedText: string, decodedResult: any) => void,
            onScanFailure?: (errorMessage: string) => void
        ): void;
        clear(): Promise<void>;
    }
}
