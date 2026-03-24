export interface ApiLogEntry {
  id: string;
  timestamp: Date;
  method: string;
  url: string;
  requestBody?: any;
  responseStatus?: number;
  responseBody?: any;
  error?: string;
  duration?: number;
}

class ApiClient {
  private static instance: ApiClient;
  private logs: ApiLogEntry[] = [];
  private listeners: ((entry: ApiLogEntry) => void)[] = [];

  private constructor() {}

  static getInstance() {
    if (!ApiClient.instance) {
      ApiClient.instance = new ApiClient();
    }
    return ApiClient.instance;
  }

  subscribe(listener: (entry: ApiLogEntry) => void) {
    this.listeners.push(listener);
    return () => {
      this.listeners = this.listeners.filter(l => l !== listener);
    };
  }

  async fetch(url: string, options: RequestInit & { silent?: boolean } = {}): Promise<Response> {
    const id = Math.random().toString(36).substring(7);
    const start = Date.now();
    const entry: ApiLogEntry = {
      id,
      timestamp: new Date(),
      method: options.method || 'GET',
      url,
      requestBody: options.body ? this.safeParse(options.body) : undefined,
    };

    if (!options.silent) {
      this.notify(entry);
    }


    try {
      const response = await fetch(url, options);
      const duration = Date.now() - start;
      
      // Clone response to read body without consuming it
      const clonedResponse = response.clone();
      let responseBody;
      try {
        const text = await clonedResponse.text();
        responseBody = this.safeParse(text);
      } catch (e) {
        responseBody = '[Binary or Unparseable]';
      }

      const updatedEntry: ApiLogEntry = {
        ...entry,
        responseStatus: response.status,
        responseBody,
        duration,
      };

      if (!options.silent) {
        this.notify(updatedEntry);
      }
      return response;
    } catch (error) {
      const duration = Date.now() - start;
      const updatedEntry: ApiLogEntry = {
        ...entry,
        error: error instanceof Error ? error.message : String(error),
        duration,
      };
      if (!options.silent) {
        this.notify(updatedEntry);
      }
      throw error;
    }

  }

  private notify(entry: ApiLogEntry) {
    // Update existing entry if ID matches, else add new
    const index = this.logs.findIndex(l => l.id === entry.id);
    if (index >= 0) {
      this.logs[index] = entry;
    } else {
      this.logs.unshift(entry);
      if (this.logs.length > 100) this.logs.pop(); // Keep last 100 calls
    }
    
    // Broadcast via CustomEvent for non-React contexts if needed
    window.dispatchEvent(new CustomEvent('api-log-updated', { detail: entry }));
    
    // Also notify direct subscribers (React hooks)
    this.listeners.forEach(l => l(entry));
  }

  getLogs() {
    return [...this.logs];
  }

  private safeParse(data: any) {
    if (typeof data !== 'string') return data;
    try {
      return JSON.parse(data);
    } catch {
      return data;
    }
  }
}

export const apiClient = ApiClient.getInstance();
