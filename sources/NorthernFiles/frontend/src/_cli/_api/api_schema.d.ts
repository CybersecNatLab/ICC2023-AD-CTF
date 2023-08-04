/**
 * This file was auto-generated by openapi-typescript.
 * Do not make direct changes to the file.
 */


export interface paths {
  "/files": {
    get: operations["getFiles"];
    post: operations["postFiles"];
  };
  "/user": {
    get: operations["getUser"];
  };
  "/files/{id}": {
    get: operations["getFilesId"];
    put: operations["putFilesId"];
  };
  "/files/{id}/share": {
    post: operations["postFilesIdShare"];
  };
}

export type webhooks = Record<string, never>;

export interface components {
  schemas: {
    Model1: {
      id: string;
      name: string;
      owner: string;
      mime_type: string;
      metadata?: string;
      key: string;
    };
    Model2: (components["schemas"]["Model1"])[];
    Model3: {
      id: string;
      username: string;
      name: string;
    };
    Model4: {
      id: string;
      name: string;
      owner: string;
      mime_type: string;
      metadata?: string;
      key: string;
    };
    Model5: {
      name: string;
      mime_type: string;
      metadata: string;
      key: string;
    };
    Model6: {
      id: string;
      upload_url: string;
      token: string;
    };
    Model7: {
      user: string;
      key: string;
    };
    Model8: Record<string, never>;
    Model9: {
      token: string;
    };
    Model10: {
      details: string;
    };
  };
  responses: never;
  parameters: never;
  requestBodies: never;
  headers: never;
  pathItems: never;
}

export type external = Record<string, never>;

export interface operations {

  getFiles: {
    responses: {
      /** @description Successful */
      200: {
        content: {
          "application/json": components["schemas"]["Model2"];
        };
      };
    };
  };
  postFiles: {
    requestBody?: {
      content: {
        "application/json": components["schemas"]["Model5"];
      };
    };
    responses: {
      /** @description Successful */
      200: {
        content: {
          "application/json": components["schemas"]["Model6"];
        };
      };
    };
  };
  getUser: {
    responses: {
      /** @description Successful */
      200: {
        content: {
          "application/json": components["schemas"]["Model3"];
        };
      };
    };
  };
  getFilesId: {
    parameters: {
      path: {
        id: string;
      };
    };
    responses: {
      /** @description Successful */
      200: {
        content: {
          "application/json": components["schemas"]["Model4"];
        };
      };
    };
  };
  putFilesId: {
    parameters: {
      path: {
        id: string;
      };
    };
    requestBody?: {
      content: {
        "application/json": components["schemas"]["Model9"];
      };
    };
    responses: {
      /** @description Successful */
      200: {
        content: {
          "application/json": components["schemas"]["Model10"];
        };
      };
    };
  };
  postFilesIdShare: {
    parameters: {
      path: {
        id: string;
      };
    };
    requestBody?: {
      content: {
        "application/json": components["schemas"]["Model7"];
      };
    };
    responses: {
      /** @description Successful */
      200: {
        content: {
          "application/json": components["schemas"]["Model8"];
        };
      };
    };
  };
}
