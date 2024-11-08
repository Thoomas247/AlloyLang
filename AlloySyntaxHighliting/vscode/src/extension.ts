// The module 'vscode' contains the VS Code extensibility API
// Import the module and reference it with the alias vscode in your code below
import * as vscode from 'vscode';
import {
	generateRandomPipeName,
	LanguageClient,
	LanguageClientOptions,
	ServerOptions,
	StreamInfo,
	Trace,
} from 'vscode-languageclient/node';
import * as net from 'net';
import { ChildProcess, exec, execSync, spawn } from 'node:child_process';
import { randomInt } from 'crypto';

// This method is called when your extension is activated
// Your extension is activated the very first time the command is executed
export function activate(context: vscode.ExtensionContext) {
	//debugger;

	// Use the console to output diagnostic information (console.log) and errors (console.error)
	// This line of code will only be executed once when your extension is activated
	// console.log('Congratulations, your extension "alloylangsyntaxhighlighting" is now active!');

	const serverOptions = async () => {
		
		var inputStream = 'AlloyLanguageServer-Input' + randomInt(999);
		var outputStream = 'AlloyLanguageServer-Output' + randomInt(999);
		var cp = await exec('c:\\\\AlloyLanguageServer\\AlloyLanguageServer.exe ' + inputStream + " " + outputStream);

		// Connect to language server via pipes
		let socketreader = net.connect('\\\\.\\pipe\\' + outputStream);
		let socketwriter = net.connect('\\\\.\\pipe\\' + inputStream);
		let result: StreamInfo = {
			writer: socketwriter,
			reader: socketreader
		};
		return Promise.resolve(result);
	};

	let clientOptions: LanguageClientOptions = {
		documentSelector: [{ scheme: 'file', language: 'alloy' }],
		synchronize: {
			configurationSection: 'alloy',
			fileEvents: vscode.workspace.createFileSystemWatcher('**/.clientrc')
		}
	};

	const client = new LanguageClient('alloy-language-server', "Alloy Language Server", serverOptions, clientOptions, true);
	// client.setTrace(Trace.Verbose);
	client.start();
}

// This method is called when your extension is deactivated
export function deactivate() { }
