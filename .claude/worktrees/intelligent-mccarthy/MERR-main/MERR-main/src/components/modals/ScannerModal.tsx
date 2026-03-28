import { logger } from '@/utils/logger';
import React, { useEffect, useState, useCallback } from 'react';
import { Html5QrcodeScanner } from 'html5-qrcode';
import { useScanRateLimit } from '@/hooks/useScanRateLimit';
import { nativeScannerService } from '@/services/native-scanner.service';

interface ScannerModalProps {
    onClose: () => void;
    onScan: (code: string) => void;
    scanType: 'BIN' | 'BUCKET';
}

const ScannerModal: React.FC<ScannerModalProps> = ({ onClose, onScan, scanType }) => {
    const [manualCode, setManualCode] = useState('');
    const [showManual, setShowManual] = useState(false);
    const [cameraError, setCameraError] = useState<string | null>(null);
    const [showSuccess, setShowSuccess] = useState(false);
    const [isNative] = useState(() => nativeScannerService.isNativePlatform());
    const [nativeScanning, setNativeScanning] = useState(false);

    // 🔧 Intelligent rate limiting — same code = 3s block, different code = 500ms debounce
    const { handleScan: rateLimitedScan } = useScanRateLimit(
        (code: string) => {
            // Show visual success feedback
            setShowSuccess(true);
            setTimeout(() => setShowSuccess(false), 1500);
            onScan(code);
        },
        { sameScanCooldownMs: 3000, globalDebounceMs: 500 }
    );

    // ── Native Scanner Flow ──────────────────────
    const startNativeScan = useCallback(async () => {
        setNativeScanning(true);
        try {
            const result = await nativeScannerService.scanNative();
            if (result) {
                rateLimitedScan(result.code);
            }
        } catch (err) {
            const msg = err instanceof Error ? err.message : 'Native scanner failed';
            logger.error('[Scanner] Native scan error:', msg);
            setCameraError(msg);
        } finally {
            setNativeScanning(false);
        }
    }, [rateLimitedScan]);

    // ── Web Scanner Flow (html5-qrcode) ──────────
    useEffect(() => {
        // Skip web scanner if native or manual mode
        if (isNative || showManual) return;

        let scanner: Html5QrcodeScanner | null = null;

        try {
            scanner = new Html5QrcodeScanner(
                "reader",
                {
                    fps: 10,
                    qrbox: { width: 250, height: 250 },
                    aspectRatio: 1.0,
                    showTorchButtonIfSupported: true
                },
                /* verbose= */ false
            );

            scanner.render(
                (decodedText) => {
                    // Don't clear scanner — let rate limiter decide
                    rateLimitedScan(decodedText);
                },
                (_errorMessage) => {
                    // Scan frame error — normal, ignore
                }
            );
        } catch (e) {
            // FIX H2: Camera init failure — auto-switch to manual
            const message = e instanceof Error ? e.message : 'Camera not available';

            logger.error('[Scanner] Camera init failed:', message);
            setCameraError(message);
            setShowManual(true);
        }

        return () => {
            scanner?.clear().catch(error => logger.error("Failed to clear scanner", error));
        };
    }, [showManual, rateLimitedScan, isNative]);

    // ── Cleanup native scanner on unmount ─────────
    useEffect(() => {
        return () => {
            if (isNative) {
                nativeScannerService.stopNativeScan();
            }
        };
    }, [isNative]);

    // ── Auto-start native scan when modal opens ───
    useEffect(() => {
        if (isNative && !showManual && !nativeScanning) {
            startNativeScan();
        }
    }, [isNative, showManual, nativeScanning, startNativeScan]);

    const handleManualSubmit = (e: React.FormEvent) => {
        e.preventDefault();
        if (manualCode.trim()) {
            rateLimitedScan(manualCode.trim().toUpperCase());
            setManualCode('');
        }
    };

    return (
        <div className="fixed inset-0 z-[100] bg-black flex flex-col justify-center animate-in fade-in duration-200">
            {/* Header */}
            <div className="absolute top-0 left-0 right-0 p-4 flex justify-between items-center z-10 bg-gradient-to-b from-black/80 to-transparent">
                <div className="text-white">
                    <h2 className="text-lg font-bold flex items-center gap-2">
                        Scan {scanType}
                        {isNative && (
                            <span className="px-1.5 py-0.5 bg-emerald-500/20 border border-emerald-500/30 rounded text-[10px] font-bold text-emerald-400">
                                NATIVE
                            </span>
                        )}
                    </h2>
                    <p className="text-xs text-text-disabled">
                        {isNative
                            ? 'Using hardware-accelerated scanner'
                            : 'Align QR code within frame'}
                    </p>
                </div>
                <button
                    onClick={onClose}
                    className="size-10 bg-white/10 backdrop-blur-md rounded-full flex items-center justify-center text-white"
                >
                    <span className="material-symbols-outlined">close</span>
                </button>
            </div>

            {/* Camera Error Banner */}
            {cameraError && (
                <div className="absolute top-20 left-4 right-4 z-20 bg-red-500/90 backdrop-blur-md text-white px-4 py-3 rounded-xl flex items-center gap-3">
                    <span className="material-symbols-outlined text-xl">videocam_off</span>
                    <div>
                        <p className="text-sm font-bold">Camera unavailable</p>
                        <p className="text-xs opacity-80">Use manual entry below</p>
                    </div>
                </div>
            )}

            {/* ✅ Scan Success Feedback */}
            {showSuccess && (
                <div className="absolute top-20 left-4 right-4 z-20 bg-emerald-500/90 backdrop-blur-md text-white px-4 py-3 rounded-xl flex items-center gap-3 animate-slide-down">
                    <span className="material-symbols-outlined text-xl">check_circle</span>
                    <div>
                        <p className="text-sm font-bold">Scanned ✓</p>
                        <p className="text-xs opacity-80">Code registered successfully</p>
                    </div>
                </div>
            )}

            {/* Scanner Area */}
            <div className="w-full max-w-lg mx-auto relative px-4">
                {showManual ? (
                    <form onSubmit={handleManualSubmit} className="bg-black/80 backdrop-blur-md p-6 rounded-2xl border border-white/10">
                        <label className="block text-sm font-medium text-text-muted mb-2">
                            Enter {scanType} Code
                        </label>
                        <input
                            type="text"
                            value={manualCode}
                            onChange={(e) => setManualCode(e.target.value.toUpperCase())}
                            className="w-full bg-black/50 border border-white/20 rounded-xl px-4 py-3 text-white font-mono text-lg focus:border-primary outline-none mb-4"
                            placeholder="e.g. BKT-1024"
                            autoFocus
                        />
                        <button
                            type="submit"
                            className="w-full bg-primary text-white font-bold py-3 rounded-xl active:scale-95 transition-transform"
                        >
                            Submit Code
                        </button>
                    </form>
                ) : isNative ? (
                    /* Native scanner — Capacitor handles the camera feed */
                    <div className="bg-black/80 backdrop-blur-md p-8 rounded-2xl border border-white/10 text-center">
                        {nativeScanning ? (
                            <>
                                <div className="w-16 h-16 border-4 border-emerald-500 border-t-transparent rounded-full animate-spin mx-auto mb-4" />
                                <p className="text-white font-semibold">Native scanner active…</p>
                                <p className="text-sm text-white/60 mt-1">Point camera at QR code</p>
                            </>
                        ) : (
                            <>
                                <span className="material-symbols-outlined text-5xl text-emerald-400 mb-4 block">qr_code_scanner</span>
                                <button
                                    onClick={startNativeScan}
                                    className="bg-emerald-500 text-white font-bold py-3 px-8 rounded-xl active:scale-95 transition-transform"
                                >
                                    Scan Again
                                </button>
                            </>
                        )}
                    </div>
                ) : (
                    /* Web scanner — html5-qrcode */
                    <div className="rounded-2xl overflow-hidden bg-black border border-white/20 shadow-2xl relative">
                        <div id="reader" className="w-full h-full"></div>
                    </div>
                )}
            </div>

            {/* Footer Actions */}
            <div className="absolute bottom-10 left-0 right-0 px-6 text-center">
                <button
                    onClick={() => { setShowManual(!showManual); setCameraError(null); }}
                    className="text-sm font-medium text-white/80 underline decoration-white/30 underline-offset-4"
                >
                    {showManual ? "Switch to Camera" : "Problem scanning? Enter code manually"}
                </button>
            </div>

            <style>{`
                #reader__scan_region { background: transparent !important; }
                #reader__dashboard_section_csr button { 
                    background: white; color: black; border: none; padding: 8px 16px; border-radius: 8px; font-weight: bold; margin-top: 10px;
                }
                #reader__dashboard_section_swaplink { display: none !important; }
                /* Native scanner: transparent body for camera preview */
                body.scanner-active { background: transparent !important; }
                body.scanner-active > *:not(.scanner-overlay) { opacity: 0.1; }
            `}</style>
        </div>
    );
};

export default ScannerModal;