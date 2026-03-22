import re
from pathlib import Path

target_file = Path(r"c:\Jay\_Plugin\compress\desktop_app\src\components\AIChatPanel.tsx")
content = target_file.read_text(encoding="utf-8")

old_try_block = """        try {
            const res = await fetch(`${API}/api/chat`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: requestMessage,
                    history: historyPayload,
                    web_search: webEnabled,
                    workspace_root: projectRoot || undefined,
                }),
            })

            if (!res.ok) {
                setMessages(prev => [...prev, {
                    role: 'assistant',
                    content: `The local connector returned status ${res.status}. Check the Python connector and C++ brain processes.`,
                    timestamp: new Date(),
                }])
                return
            }

            const data = await res.json()
            let replyContent = data.response || data.reply || data.content || JSON.stringify(data)"""

new_try_block = """        const assistantMsgId = `msg-${Date.now()}`
        setMessages(prev => [...prev, {
            role: 'assistant',
            content: '',
            timestamp: new Date(),
            messageId: assistantMsgId,
        }])

        try {
            const res = await fetch(`${API}/api/chat_stream`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: requestMessage,
                    history: historyPayload,
                    web_search: webEnabled,
                    workspace_root: projectRoot || undefined,
                }),
            })

            if (!res.ok) {
                setMessages(prev => prev.map(m => m.messageId === assistantMsgId ? {
                    ...m,
                    content: `The local connector returned status ${res.status}. Check the Python connector and C++ brain processes.`
                } : m))
                return
            }

            const reader = res.body?.getReader()
            const decoder = new TextDecoder()
            let partialContent = ''
            let finalData: any = null
            let sseBuffer = ''

            if (reader) {
                while (true) {
                    const { done, value } = await reader.read()
                    if (done) break
                    sseBuffer += decoder.decode(value, { stream: true })

                    let newlineIndex
                    while ((newlineIndex = sseBuffer.indexOf('\\n\\n')) >= 0) {
                        const line = sseBuffer.slice(0, newlineIndex).trim()
                        sseBuffer = sseBuffer.slice(newlineIndex + 2)

                        if (line.startsWith('data: ')) {
                            const jsonStr = line.slice(6).trim()
                            if (jsonStr) {
                                try {
                                    const event = JSON.parse(jsonStr)
                                    if (event.type === 'chunk' && event.content) {
                                        partialContent += event.content
                                        setMessages(prev => prev.map(m =>
                                            m.messageId === assistantMsgId ? { ...m, content: partialContent } : m
                                        ))
                                    } else if (event.type === 'final' && event.data) {
                                        finalData = event.data
                                    } else if (event.type === 'error') {
                                        partialContent += `\\n[Error: ${event.error}]`
                                        setMessages(prev => prev.map(m =>
                                            m.messageId === assistantMsgId ? { ...m, content: partialContent } : m
                                        ))
                                    }
                                } catch (e) {
                                    console.error('SSE parse error:', e, jsonStr)
                                }
                            }
                        }
                    }
                }
            }

            const data = finalData || {}
            let replyContent = data.response || data.reply || data.content || partialContent || JSON.stringify(data)"""

content = content.replace(old_try_block, new_try_block)

old_end_block = """            setMessages(prev => [...prev, {
                role: 'assistant',
                content: replyContent,
                timestamp: new Date(),
                tool: toolUsed,
                messageId: `msg-${Date.now()}`,
                feedback: null,
                command: proposedCommand,
                browserUrl: proposedUrl,
                confidence,
                sources: sourceLines,
                flowSections,
                analysis,
                findings,
                testGaps,
                proposedCode,
                targetFile,
                targetPath,
                appliedContent,
                validation,
            }])
            setAttachedFiles([])
        } catch (err) {
            setMessages(prev => [...prev, {
                role: 'assistant',
                content: `Could not reach the local connector at ${API}. Make sure the Python connector is running.`,
                timestamp: new Date(),
            }])
        } finally {
            setIsTyping(false)
        }"""

new_end_block = """            setMessages(prev => prev.map(m => m.messageId === assistantMsgId ? {
                ...m,
                content: replyContent,
                tool: toolUsed,
                feedback: null,
                command: proposedCommand,
                browserUrl: proposedUrl,
                confidence,
                sources: sourceLines,
                flowSections,
                analysis,
                findings,
                testGaps,
                proposedCode,
                targetFile,
                targetPath,
                appliedContent,
                validation,
            } : m))
            setAttachedFiles([])
        } catch (err) {
            setMessages(prev => {
                const exists = prev.some(m => m.messageId === assistantMsgId)
                if (exists) {
                    return prev.map(m => m.messageId === assistantMsgId ? {
                        ...m,
                        content: `Could not reach the local connector at ${API}. Make sure the Python connector is running.`
                    } : m)
                } else {
                    return [...prev, {
                        role: 'assistant',
                        content: `Could not reach the local connector at ${API}. Make sure the Python connector is running.`,
                        timestamp: new Date(),
                    }]
                }
            })
        } finally {
            setIsTyping(false)
        }"""

content = content.replace(old_end_block, new_end_block)

target_file.write_text(content, encoding="utf-8")
print("Successfully updated AIChatPanel.tsx")
