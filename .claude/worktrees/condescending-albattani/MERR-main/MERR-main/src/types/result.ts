/**
 * Result<T> pattern — Uniform error handling across services
 * 
 * Inspired by Rust/Go: forces callers to check `result.ok` before accessing data.
 * Replaces ad-hoc try/catch + return null patterns.
 */

/** Typed error with machine-readable code */
export class ServiceError {
    constructor(
        /** Machine-readable error code, e.g. 'PICKER_NOT_FOUND', 'NETWORK_ERROR' */
        public readonly code: string,
        /** Human-readable description */
        public readonly message: string,
        /** Original error/exception if any */
        public readonly cause?: unknown,
    ) { }

    toString(): string {
        return `[${this.code}] ${this.message}`;
    }
}

/** Discriminated union: either success with data, or failure with typed error */
export type Result<T> =
    | { ok: true; data: T }
    | { ok: false; error: ServiceError };

/** Create a success result */
export const Ok = <T>(data: T): Result<T> => ({ ok: true, data });

/** Create an error result */
export const Err = <T>(code: string, message: string, cause?: unknown): Result<T> =>
    ({ ok: false, error: new ServiceError(code, message, cause) });

/**
 * Wrap an async operation in a Result.
 * Catches any thrown error and converts it to Err().
 * 
 * Usage:
 * ```ts
 * const result = await tryCatch('FETCH_PICKERS', () => supabase.from('pickers').select('*'));
 * if (!result.ok) { logger.error(result.error); return; }
 * const pickers = result.data;
 * ```
 */
export async function tryCatch<T>(
    errorCode: string,
    fn: () => Promise<T>,
): Promise<Result<T>> {
    try {
        const data = await fn();
        return Ok(data);
    } catch (e) {
        const message = e instanceof Error ? e.message : 'Unknown error';
        return Err(errorCode, message, e);
    }
}
