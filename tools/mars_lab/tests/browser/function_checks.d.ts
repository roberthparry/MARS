/**
 * @file function_checks.d.ts
 * @brief Browser entry-point declaration for function-card regressions.
 *
 * Declares compile-time fixture shapes and browser harness entry points for these checks.
 * The declarations add no runtime behaviour and remain separate from production contracts.
 */

interface Window { checkLabFunction(): Promise<void>; }
