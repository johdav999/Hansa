export class WorkerError extends Error {
  constructor(code, message, { remedy = "", retryable = false, details = undefined } = {}) {
    super(message);
    this.name = "WorkerError";
    this.code = code;
    this.remedy = remedy;
    this.retryable = retryable;
    this.details = details;
  }
}

export function structuredError(error) {
  if (error instanceof WorkerError) {
    return {
      code: error.code,
      message: error.message,
      remedy: error.remedy,
      retryable: error.retryable,
      ...(error.details === undefined ? {} : { details: error.details }),
    };
  }
  return {
    code: "InternalError",
    message: "The generation worker could not complete the operation.",
    remedy: "Inspect the redacted worker log and retry the job.",
    retryable: true,
  };
}
