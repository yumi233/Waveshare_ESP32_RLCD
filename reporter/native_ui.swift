// Copyright (c) 2026 黑沐. Licensed under the MIT License.
import Cocoa
import Foundation
import ServiceManagement
import CoreImage

final class Canvas: NSView {
    override var isFlipped: Bool { true }
}

final class ReporterApp: NSObject, NSApplicationDelegate, NSWindowDelegate {
    var window: NSWindow!
    var statusItem: NSStatusItem!
    var menuQuota: NSMenuItem!
    var backend: Process?
    var timer: Timer?
    var polling = false
    var initialCheck = true
    var stopping = false
    var quitting = false
    var lastSuccess: Date?
    var launchedAt: Date?
    let port = 18765
    let ink = NSColor(calibratedRed: 0.13, green: 0.20, blue: 0.23, alpha: 1)
    let muted = NSColor(calibratedRed: 0.43, green: 0.49, blue: 0.50, alpha: 1)
    let green = NSColor(calibratedRed: 0.12, green: 0.46, blue: 0.36, alpha: 1)
    var statusLabel: NSTextField!
    var detailLabel: NSTextField!
    var hostLabel: NSTextField!
    var cpu: NSTextField!
    var gpu: NSTextField!
    var cpuTemperature: NSTextField!
    var gpuTemperature: NSTextField!
    var memory: NSTextField!
    var disk: NSTextField!
    var upload: NSTextField!
    var download: NSTextField!
    var agent: NSTextField!
    var quota: NSTextField!
    var device: NSTextField!
    var musicTitle: NSTextField!
    var musicDetail: NSTextField!
    var updated: NSTextField!
    var toggle: NSButton!
    var diagnostic: NSButton!
    var loginSwitch: NSSwitch!
    var loginStatus: NSTextField!
    var loginSettings: NSButton!
    var electricitySummary: NSTextField!
    var electricityForecast: NSTextField!
    var electricityButton: NSButton!
    var electricityWindow: NSWindow?
    var electricityQR: NSImageView!
    var electricityPlaceholder: NSTextField!
    var electricityBalance: NSTextField!
    var electricityMeta: NSTextField!
    var electricityCountdown: NSTextField!
    var electricityBuilding: NSTextField!
    var electricityRoom: NSTextField!
    var electricityThreshold: NSTextField!
    var electricityAmount100: NSButton!
    var electricityAmount50: NSButton!
    var electricityGenerate: NSButton!
    var electricityCheck: NSButton!
    var electricityLater: NSButton!
    var electricityAlertID: Int?
    var electricityVisible = false
    var electricityAmount = 100
    var latestElectricity: [String: Any] = [:]
    var electricityDismissed = false
    var electricityPosting = false
    var electricityRequestError: String?
    let session: URLSession = {
        let config = URLSessionConfiguration.ephemeral
        config.timeoutIntervalForRequest = 2
        config.timeoutIntervalForResource = 3
        config.connectionProxyDictionary = [:]
        config.requestCachePolicy = .reloadIgnoringLocalCacheData
        return URLSession(configuration: config)
    }()

    var dataDirectory: URL {
        if let override = ProcessInfo.processInfo.environment["SYNA_REPORTER_DATA_DIR"] {
            return URL(fileURLWithPath: override)
        }
        return FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/Application Support/SynaReporter", isDirectory: true)
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.accessory)
        NSApp.appearance = NSAppearance(named: .aqua)
        if CommandLine.arguments.contains("--installer-enable-login") {
            if #available(macOS 13.0, *) {
                do {
                    // Installation may move the app from a development folder
                    // or an older location. Refresh the registered path.
                    if SMAppService.mainApp.status == .enabled { try SMAppService.mainApp.unregister() }
                    try SMAppService.mainApp.register()
                }
                catch { NSLog("Installer login registration: %@", error.localizedDescription) }
            }
        }
        buildMenu()
        buildWindow()
        buildStatusItem()
        refresh()
        timer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in self?.refresh() }
    }

    func buildMenu() {
        let menu = NSMenu()
        let appItem = NSMenuItem()
        let submenu = NSMenu()
        submenu.addItem(withTitle: "显示状态窗口", action: #selector(showWindow), keyEquivalent: "0").target = self
        submenu.addItem(NSMenuItem.separator())
        submenu.addItem(withTitle: "退出 Syna Reporter", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        appItem.submenu = submenu
        menu.addItem(appItem)
        NSApp.mainMenu = menu
    }

    func buildStatusItem() {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        if let button = statusItem.button {
            button.image = NSImage(systemSymbolName: "waveform", accessibilityDescription: "Syna Reporter")
            button.image?.isTemplate = true
            button.toolTip = "Syna Reporter · 电脑状态与 Codex 额度"
        }
        let menu = NSMenu()
        menu.autoenablesItems = false
        menuQuota = NSMenuItem(title: "Codex 额度读取中…", action: nil, keyEquivalent: "")
        menuQuota.isEnabled = false
        menu.addItem(menuQuota)
        menu.addItem(NSMenuItem.separator())
        menu.addItem(withTitle: "显示状态窗口", action: #selector(showWindow), keyEquivalent: "").target = self
        menu.addItem(withTitle: "查看诊断", action: #selector(openDiagnostic), keyEquivalent: "").target = self
        menu.addItem(withTitle: "打开日志", action: #selector(openLogs), keyEquivalent: "").target = self
        menu.addItem(NSMenuItem.separator())
        menu.addItem(withTitle: "退出 Reporter", action: #selector(quitReporter), keyEquivalent: "").target = self
        statusItem.menu = menu
    }

    @objc func quitReporter() { NSApp.terminate(nil) }

    @discardableResult
    func text(_ parent: NSView, _ value: String, _ x: CGFloat, _ y: CGFloat,
              _ width: CGFloat, _ height: CGFloat, _ size: CGFloat = 14,
              _ weight: NSFont.Weight = .regular, _ color: NSColor? = nil) -> NSTextField {
        let label = NSTextField(labelWithString: value)
        label.frame = NSRect(x: x, y: y, width: width, height: height)
        label.font = .systemFont(ofSize: size, weight: weight)
        label.textColor = color ?? ink
        label.lineBreakMode = .byTruncatingTail
        label.isSelectable = true
        parent.addSubview(label)
        return label
    }

    func card(_ parent: NSView, _ rect: NSRect, _ tint: NSColor = .white) -> Canvas {
        let view = Canvas(frame: rect)
        view.wantsLayer = true
        view.layer?.backgroundColor = tint.cgColor
        view.layer?.cornerRadius = 16
        parent.addSubview(view)
        return view
    }

    func metric(_ parent: NSView, _ name: String, _ x: CGFloat) -> NSTextField {
        let view = card(parent, NSRect(x: x, y: 210, width: 166, height: 108))
        text(view, name, 20, 16, 126, 22, 12, .medium, muted)
        return text(view, "—", 20, 40, 126, 42, 28, .semibold)
    }

    func buildWindow() {
        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 760, height: 750),
                          styleMask: [.titled, .closable, .miniaturizable], backing: .buffered, defer: false)
        window.title = "Syna Reporter"
        window.isReleasedWhenClosed = false
        window.titlebarAppearsTransparent = true
        window.backgroundColor = NSColor(calibratedRed: 0.95, green: 0.96, blue: 0.95, alpha: 1)
        window.appearance = NSAppearance(named: .aqua)
        let page = Canvas(frame: NSRect(x: 0, y: 0, width: 760, height: 750))
        window.contentView = page
        text(page, "Syna Reporter", 28, 24, 470, 38, 29, .semibold)
        text(page, "你的电脑与 Syna，现在一目了然。", 29, 64, 650, 23, 13, .regular, muted)
        text(page, "MAC 状态中心", 576, 34, 156, 23, 11, .semibold, muted).alignment = .right

        let banner = card(page, NSRect(x: 28, y: 100, width: 704, height: 96),
                          NSColor(calibratedRed: 0.88, green: 0.94, blue: 0.91, alpha: 1))
        statusLabel = text(banner, "正在检查服务…", 22, 17, 440, 33, 24, .semibold, green)
        detailLabel = text(banner, "连接本机 Reporter", 23, 58, 658, 24, 12, .regular, muted)
        hostLabel = text(banner, "", 415, 22, 266, 25, 12, .medium, muted)
        hostLabel.alignment = .right

        cpu = metric(page, "CPU 使用率", 28)
        gpu = metric(page, "GPU 使用率", 207)
        memory = metric(page, "内存使用率", 386)
        disk = metric(page, "磁盘空间占用", 565)
        cpuTemperature = text(cpu.superview!, "温度 —", 20, 86, 126, 18, 11, .regular, muted)
        gpuTemperature = text(gpu.superview!, "温度 —", 20, 86, 126, 18, 11, .regular, muted)
        cpuTemperature.toolTip = "CPU 温度传感器平均值，每 5 秒更新"
        gpuTemperature.toolTip = "GPU 温度传感器平均值，每 5 秒更新"

        let network = card(page, NSRect(x: 28, y: 332, width: 704, height: 76))
        text(network, "实时网络", 20, 14, 160, 22, 12, .medium, muted)
        upload = text(network, "↑  —", 20, 38, 320, 29, 19, .medium)
        download = text(network, "↓  —", 363, 38, 320, 29, 19, .medium)

        let codex = card(page, NSRect(x: 28, y: 422, width: 346, height: 102))
        text(codex, "CODEX", 20, 15, 160, 21, 11, .semibold, muted)
        agent = text(codex, "等待数据", 20, 39, 306, 27, 20, .semibold)
        quota = text(codex, "短周期剩余 —    ·    周额度剩余 —", 20, 73, 306, 23, 12, .regular, muted)

        let music = card(page, NSRect(x: 388, y: 422, width: 344, height: 102))
        text(music, "网易云音乐", 20, 15, 304, 21, 11, .semibold, muted)
        musicTitle = text(music, "等待播放", 20, 39, 304, 27, 16, .semibold)
        musicDetail = text(music, "打开网易云并播放歌曲", 20, 73, 304, 23, 11, .regular, muted)

        let board = card(page, NSRect(x: 28, y: 538, width: 704, height: 72))
        text(board, "开发板通信", 20, 12, 180, 22, 12, .medium, muted)
        device = text(board, "等待开发板请求", 20, 37, 320, 24, 14, .medium)
        electricitySummary = text(board, "电费：读取中…", 360, 12, 205, 22, 12, .medium, muted)
        electricitySummary.alignment = .right
        electricityForecast = text(board, "预计天数统计中", 360, 37, 205, 22, 12, .regular, muted)
        electricityForecast.alignment = .right
        electricityButton = NSButton(title: "电费 / 充值", target: self, action: #selector(openElectricity))
        electricityButton.bezelStyle = .rounded
        electricityButton.frame = NSRect(x: 573, y: 18, width: 110, height: 34)
        board.addSubview(electricityButton)

        toggle = NSButton(title: "正在检查…", target: self, action: #selector(toggleService))
        toggle.bezelStyle = .rounded
        toggle.controlSize = .large
        toggle.frame = NSRect(x: 24, y: 626, width: 133, height: 34)
        toggle.isEnabled = false
        page.addSubview(toggle)
        diagnostic = NSButton(title: "查看诊断", target: self, action: #selector(openDiagnostic))
        diagnostic.bezelStyle = .rounded
        diagnostic.frame = NSRect(x: 169, y: 626, width: 108, height: 34)
        page.addSubview(diagnostic)
        let logs = NSButton(title: "打开日志", target: self, action: #selector(openLogs))
        logs.bezelStyle = .rounded
        logs.frame = NSRect(x: 288, y: 626, width: 108, height: 34)
        page.addSubview(logs)
        let author = NSButton(title: "关于作者", target: self, action: #selector(aboutAuthor))
        author.bezelStyle = .rounded
        author.frame = NSRect(x: 407, y: 626, width: 108, height: 34)
        page.addSubview(author)
        updated = text(page, "等待首次更新", 526, 635, 206, 22, 11, .regular, muted)
        updated.alignment = .right
        loginSwitch = NSSwitch(frame: NSRect(x: 28, y: 674, width: 40, height: 24))
        loginSwitch.target = self
        loginSwitch.action = #selector(toggleLogin)
        loginSwitch.setAccessibilityLabel("登录时自动启动")
        page.addSubview(loginSwitch)
        text(page, "登录时自动启动", 78, 676, 160, 22, 13, .medium)
        loginStatus = text(page, "", 246, 678, 350, 22, 11, .regular, muted)
        loginSettings = NSButton(title: "系统登录项", target: self, action: #selector(openLoginSettings))
        loginSettings.bezelStyle = .rounded
        loginSettings.frame = NSRect(x: 618, y: 671, width: 118, height: 32)
        page.addSubview(loginSettings)
        refreshLoginStatus()
        text(page, "关闭窗口后驻留菜单栏 · 菜单栏可重新打开或退出", 29, 718, 700, 20, 11, .regular, muted)
        window.center()
    }

    @objc func showWindow() {
        window.deminiaturize(nil)
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        showWindow()
        return false
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { false }

    func refresh() {
        refreshLoginStatus()
        guard !polling && !quitting else { return }
        polling = true
        let url = URL(string: "http://127.0.0.1:\(port)/api/v1/status")!
        session.dataTask(with: url) { [weak self] data, response, error in
            let json = data.flatMap { try? JSONSerialization.jsonObject(with: $0) } as? [String: Any]
            DispatchQueue.main.async {
                guard let self = self else { return }
                self.polling = false
                if let body = json, (response as? HTTPURLResponse)?.statusCode == 200,
                   body["reporter_id"] is String, body["online"] as? Bool == true {
                    self.initialCheck = false
                    self.lastSuccess = Date()
                    self.render(body)
                } else {
                    self.offline()
                    if self.initialCheck {
                        self.initialCheck = false
                        self.startService()
                    }
                }
            }
        }.resume()
    }

    func percent(_ value: Any?) -> String {
        guard let number = value as? NSNumber else { return "—" }
        return String(format: "%.1f%%", number.doubleValue)
    }

    func rate(_ value: Any?) -> String {
        guard let number = value as? NSNumber else { return "—" }
        let speed = max(0, number.doubleValue)
        return speed >= 1048576 ? String(format: "%.2f MB/s", speed / 1048576)
            : String(format: "%.1f KB/s", speed / 1024)
    }

    func render(_ body: [String: Any]) {
        statusLabel.stringValue = stopping ? "正在停止…" : "●  运行中"
        statusLabel.textColor = green
        hostLabel.stringValue = body["computer_name"] as? String ?? "这台 Mac"
        detailLabel.stringValue = backend == nil ? "服务由其他进程运行；当前窗口用于查看状态。" : "正在采集电脑状态，并向同一网络中的开发板提供数据。"
        let performance = body["performance"] as? [String: Any] ?? [:]
        cpu.stringValue = percent(performance["cpu_percent"])
        gpu.stringValue = percent(performance["gpu_percent"])
        func temperature(_ value: Any?) -> String {
            guard let number = value as? NSNumber else { return "温度 —" }
            return String(format: "温度 %.1f°C", number.doubleValue)
        }
        cpuTemperature.stringValue = temperature(performance["cpu_temp_c"])
        gpuTemperature.stringValue = temperature(performance["gpu_temp_c"])
        memory.stringValue = percent(performance["memory_percent"])
        disk.stringValue = percent(performance["disk_percent"])
        upload.stringValue = "↑  " + rate(performance["upload_bytes_per_sec"])
        download.stringValue = "↓  " + rate(performance["download_bytes_per_sec"])
        let state = body["agent"] as? [String: Any] ?? [:]
        let code = state["state"] as? String ?? "offline"
        let names = ["working": "工作中", "done": "已完成", "idle": "空闲", "offline": "未运行", "login_required": "请登录 Codex", "waiting": "等待输入"]
        let count = (state["active_count"] as? NSNumber)?.intValue ?? 0
        agent.stringValue = (names[code] ?? "状态不可用") + (code == "working" ? "  ·  \(count) 个任务" : "")
        let limits = body["codex_quota"] as? [String: Any] ?? [:]
        let cached = limits["stale"] as? Bool == true
        let quotaSource = limits["source"] as? String ?? "unavailable"
        var values: [String] = []
        if limits["short_remaining_percent"] is NSNumber {
            values.append("短周期剩余 " + percent(limits["short_remaining_percent"]))
        }
        if limits["week_remaining_percent"] is NSNumber {
            values.append("周额度剩余 " + percent(limits["week_remaining_percent"]))
        }
        let unavailable = quotaSource == "login_required" ? "请先登录 Codex"
            : (quotaSource == "codex_not_found" ? "未找到 Codex 客户端" : "额度暂不可用，正在重试")
        let quotaText = values.isEmpty ? unavailable : values.joined(separator: "  ·  ")
        quota.stringValue = quotaText + (cached ? "（缓存）" : "")
        quota.toolTip = quota.stringValue
        menuQuota?.title = "Codex · " + quota.stringValue
        let media = body["media"] as? [String: Any] ?? [:]
        if media["available"] as? Bool == true {
            musicTitle.stringValue = media["title"] as? String ?? ""
            musicTitle.toolTip = musicTitle.stringValue
            let singer = media["artist"] as? String ?? ""
            let status = media["playback_status"] as? String == "playing" ? "播放中" : "已暂停"
            func clock(_ value: Any?) -> String {
                let seconds = max(0, (value as? NSNumber)?.intValue ?? 0)
                return String(format: "%d:%02d", seconds / 60, seconds % 60)
            }
            musicDetail.stringValue = "\(status) · \(clock(media["position_seconds"]))/\(clock(media["duration_seconds"])) · \(singer)"
            musicDetail.toolTip = musicDetail.stringValue
        } else {
            musicTitle.stringValue = "未获取到歌曲"
            musicTitle.toolTip = nil
            musicDetail.stringValue = "请在网易云播放歌曲"
            musicDetail.toolTip = nil
        }
        let peers = body["devices"] as? [[String: Any]] ?? []
        let ips = peers.compactMap { $0["ip"] as? String }.joined(separator: "、")
        device.stringValue = peers.isEmpty ? "尚未收到请求，请确认开发板与 Mac 连接同一 Wi-Fi。"
            : "已收到 \(peers.count) 台开发板请求  ·  \(ips)"
        device.toolTip = "表示最近 15 秒收到发现请求，不代表开发板已选中本机。"
        renderElectricity(body["electricity"] as? [String: Any] ?? [:])
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm:ss"
        updated.stringValue = "最近更新  " + formatter.string(from: Date())
        toggle.title = backend == nil ? "由其他进程运行" : "停止服务"
        toggle.isEnabled = backend != nil && !stopping
        diagnostic.isEnabled = true
    }

    func offline() {
        let starting = backend?.isRunning == true && launchedAt.map { Date().timeIntervalSince($0) < 8 } == true
        statusLabel.stringValue = stopping ? "正在停止…" : (starting ? "正在启动…" : (backend?.isRunning == true ? "○  服务连接异常" : "○  服务未运行"))
        statusLabel.textColor = starting ? green : .systemOrange
        detailLabel.stringValue = starting ? "正在初始化采集与网络服务。" : "暂无实时数据。点击启动服务，或打开日志查看原因。"
        for label in [cpu, gpu, memory, disk] { label?.stringValue = "—" }
        cpuTemperature.stringValue = "温度 —"
        gpuTemperature.stringValue = "温度 —"
        upload.stringValue = "↑  —"
        download.stringValue = "↓  —"
        agent.stringValue = "等待服务启动"
        musicTitle.stringValue = "等待服务启动"
        musicDetail.stringValue = "—"
        musicTitle.toolTip = nil
        musicDetail.toolTip = nil
        quota.stringValue = "短周期剩余 —    ·    周额度剩余 —"
        menuQuota?.title = "Reporter 暂未连接服务"
        device.stringValue = "服务停止时，不向开发板发送数据。"
        electricitySummary.stringValue = "电费：服务未连接"
        electricityForecast.stringValue = "预计天数：—"
        electricityButton.isEnabled = false
        if electricityVisible {
            electricityQR.image = nil
            electricityMeta.stringValue = "服务连接中断，请重试。"
            electricityCountdown.stringValue = "二维码已隐藏"
            electricityGenerate.isEnabled = false
        }
        updated.stringValue = lastSuccess == nil ? "尚未连接服务" : "连接中断，已清除旧数据"
        toggle.title = backend?.isRunning == true ? "停止服务" : "启动服务"
        toggle.isEnabled = !stopping && !initialCheck
        diagnostic.isEnabled = false
    }

    func renderElectricity(_ electricity: [String: Any]) {
        latestElectricity = electricity
        electricityButton.isEnabled = true
        let configured = electricity["configured"] as? Bool ?? false
        let available = electricity["available"] as? Bool ?? false
        let stale = electricity["stale"] as? Bool ?? false
        if !configured {
            electricitySummary.stringValue = "宿舍用电：未配置"
        } else if available, let remaining = electricity["remaining_kwh"] as? NSNumber {
            electricitySummary.stringValue = String(format: "剩余 %.2f 度%@", remaining.doubleValue, stale ? "（缓存）" : "")
        } else {
            electricitySummary.stringValue = "宿舍用电：暂不可用"
        }
        if let days = electricity["estimated_days"] as? NSNumber, available {
            electricityForecast.stringValue = String(format: "预计可用 %.1f 天%@", days.doubleValue, stale ? "（缓存）" : "")
        } else { electricityForecast.stringValue = configured ? "预计天数统计中" : "" }
        let alert = electricity["show_alert"] as? Bool ?? false
        if !alert { electricityDismissed = false; electricityAlertID = nil }
        if alert && !electricityDismissed && electricityAlertID == nil {
            electricityAlertID = (electricity["alert_id"] as? NSNumber)?.intValue ?? 0
            electricityAmount = (electricity["payment_amount_yuan"] as? NSNumber)?.intValue == 50 ? 50 : 100
            openElectricity()
        }
        if electricityVisible { updateElectricityWindow(electricity) }
    }

    func postElectricity(_ path: String, body: [String: Any] = [:]) {
        guard !electricityPosting else { return }
        var request = URLRequest(url: URL(string: "http://127.0.0.1:\(port)/api/v1/electricity/\(path)")!)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try? JSONSerialization.data(withJSONObject: body)
        electricityPosting = true
        electricityRequestError = nil
        if path == "recharge" { electricityQR?.image = nil }
        session.dataTask(with: request) { [weak self] data, response, error in
            let code = (response as? HTTPURLResponse)?.statusCode ?? 0
            let result = data.flatMap { try? JSONSerialization.jsonObject(with: $0) as? [String: Any] }
            DispatchQueue.main.async {
                guard let self else { return }
                self.electricityPosting = false
                if error != nil || code != 200 {
                    self.electricityRequestError = code == 409 ? "请等待余额更新或订单生成完成后重试。" : "操作失败，请检查连接后重试。"
                    self.electricityQR?.image = nil
                } else if let snapshot = result?["electricity"] as? [String: Any] {
                    self.renderElectricity(snapshot)
                }
                self.refresh()
            }
        }.resume()
    }

    @objc func openElectricity() {
        if electricityWindow == nil { buildElectricityWindow() }
        guard let electricityWindow else { return }
        if !electricityVisible {
            let building = (latestElectricity["building"] as? NSNumber)?.intValue ?? 0
            let room = (latestElectricity["room"] as? NSNumber)?.intValue ?? 0
            electricityBuilding.stringValue = building > 0 ? String(building) : ""
            electricityRoom.stringValue = room > 0 ? String(room) : ""
            electricityThreshold.stringValue = String(format: "%.0f", (latestElectricity["threshold_kwh"] as? NSNumber)?.doubleValue ?? 10)
        }
        electricityVisible = true
        electricityAmount100.state = electricityAmount == 100 ? .on : .off
        electricityAmount50.state = electricityAmount == 50 ? .on : .off
        updateElectricityWindow(latestElectricity)
        electricityWindow.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    func buildElectricityWindow() {
        let panel = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 620, height: 530),
                             styleMask: [.titled, .closable], backing: .buffered, defer: false)
        panel.title = "夏柠 · 宿舍用电"
        panel.isReleasedWhenClosed = false
        panel.backgroundColor = NSColor(calibratedRed: 0.96, green: 0.97, blue: 0.96, alpha: 1)
        panel.delegate = self
        let page = Canvas(frame: NSRect(x: 0, y: 0, width: 620, height: 530))
        panel.contentView = page
        text(page, "宿舍用电", 28, 24, 290, 35, 27, .semibold)
        electricityBalance = text(page, "剩余电量：—", 30, 78, 300, 30, 20, .medium, green)
        electricityMeta = text(page, "等待数据", 30, 119, 300, 72, 13, .regular, muted)
        electricityMeta.maximumNumberOfLines = 3
        electricityMeta.cell?.wraps = true
        _ = card(page, NSRect(x: 370, y: 24, width: 220, height: 220), .white)
        electricityPlaceholder = text(page, "生成充值码后\n用微信扫码付款", 382, 108, 196, 48, 14, .regular, muted)
        electricityPlaceholder.maximumNumberOfLines = 2
        electricityPlaceholder.alignment = .center
        electricityPlaceholder.cell?.wraps = true
        electricityQR = NSImageView(frame: NSRect(x: 370, y: 24, width: 220, height: 220))
        electricityQR.imageScaling = .scaleNone
        electricityQR.imageAlignment = .alignCenter
        electricityQR.wantsLayer = true
        electricityQR.layer?.magnificationFilter = .nearest
        electricityQR.layer?.minificationFilter = .nearest
        page.addSubview(electricityQR)
        electricityCountdown = text(page, "选择金额后生成二维码", 342, 250, 248, 24, 12, .medium, muted)
        electricityCountdown.alignment = .center
        let form = card(page, NSRect(x: 28, y: 290, width: 564, height: 116), NSColor.white)
        text(form, "充值金额", 20, 14, 90, 22, 12, .medium, muted)
        electricityAmount100 = NSButton(radioButtonWithTitle: "100 元", target: self, action: #selector(selectElectricityAmount(_:)))
        electricityAmount100.frame = NSRect(x: 112, y: 10, width: 90, height: 28)
        electricityAmount100.tag = 100
        form.addSubview(electricityAmount100)
        electricityAmount50 = NSButton(radioButtonWithTitle: "50 元", target: self, action: #selector(selectElectricityAmount(_:)))
        electricityAmount50.frame = NSRect(x: 205, y: 10, width: 85, height: 28)
        electricityAmount50.tag = 50
        form.addSubview(electricityAmount50)
        text(form, "楼栋", 20, 53, 38, 22, 12, .medium, muted)
        electricityBuilding = editableField(form, "", 58, 48, 65, 28)
        text(form, "房间", 137, 53, 38, 22, 12, .medium, muted)
        electricityRoom = editableField(form, "", 175, 48, 65, 28)
        text(form, "提醒阈值", 255, 53, 65, 22, 12, .medium, muted)
        electricityThreshold = editableField(form, "10", 320, 48, 55, 28)
        text(form, "度", 380, 53, 25, 22, 12, .medium, muted)
        let save = NSButton(title: "保存设置", target: self, action: #selector(saveElectricitySettings))
        save.bezelStyle = .rounded
        save.frame = NSRect(x: 425, y: 46, width: 115, height: 32)
        form.addSubview(save)
        electricityGenerate = NSButton(title: "生成充值码", target: self, action: #selector(generateElectricityQR))
        electricityGenerate.bezelStyle = .rounded
        electricityGenerate.frame = NSRect(x: 28, y: 430, width: 142, height: 34)
        page.addSubview(electricityGenerate)
        electricityCheck = NSButton(title: "查询支付结果", target: self, action: #selector(checkElectricityPayment))
        electricityCheck.bezelStyle = .rounded
        electricityCheck.frame = NSRect(x: 184, y: 430, width: 142, height: 34)
        page.addSubview(electricityCheck)
        electricityLater = NSButton(title: "稍后再付", target: self, action: #selector(dismissElectricity))
        electricityLater.bezelStyle = .rounded
        electricityLater.frame = NSRect(x: 340, y: 430, width: 110, height: 34)
        page.addSubview(electricityLater)
        text(page, "用微信扫描学校充值码完成付款，电量到账可能稍有延迟。", 28, 486, 564, 22, 12, .regular, muted)
        panel.center()
        electricityWindow = panel
    }

    func editableField(_ parent: NSView, _ value: String, _ x: CGFloat, _ y: CGFloat, _ width: CGFloat, _ height: CGFloat) -> NSTextField {
        let field = NSTextField(string: value)
        field.frame = NSRect(x: x, y: y, width: width, height: height)
        field.alignment = .center
        parent.addSubview(field)
        return field
    }

    @objc func selectElectricityAmount(_ sender: NSButton) {
        electricityAmount = sender.tag
        electricityAmount100.state = electricityAmount == 100 ? .on : .off
        electricityAmount50.state = electricityAmount == 50 ? .on : .off
    }

    @objc func saveElectricitySettings() {
        guard let building = Int(electricityBuilding.stringValue), (1...999).contains(building),
              let room = Int(electricityRoom.stringValue), (1...9999).contains(room),
              let threshold = Double(electricityThreshold.stringValue), threshold.isFinite, (1...100).contains(threshold) else {
            electricityRequestError = "请填写有效楼栋、房间和 1～100 度提醒阈值。"
            updateElectricityWindow(latestElectricity)
            return
        }
        postElectricity("config", body: ["building": building, "room": room, "threshold_kwh": threshold, "enabled": true])
    }

    @objc func generateElectricityQR() {
        let building = (latestElectricity["building"] as? NSNumber)?.intValue
        let room = (latestElectricity["room"] as? NSNumber)?.intValue
        let threshold = (latestElectricity["threshold_kwh"] as? NSNumber)?.doubleValue
        guard Int(electricityBuilding.stringValue) == building, Int(electricityRoom.stringValue) == room,
              Double(electricityThreshold.stringValue) == threshold else {
            electricityRequestError = "设置已修改，请先保存，等余额更新后再充值。"
            updateElectricityWindow(latestElectricity)
            return
        }
        postElectricity("recharge", body: ["amount_yuan": electricityAmount])
        updateElectricityWindow(latestElectricity)
    }

    @objc func checkElectricityPayment() { postElectricity("check") }
    @objc func dismissElectricity() {
        electricityDismissed = true
        postElectricity("dismiss")
        electricityWindow?.orderOut(nil)
        electricityVisible = false
    }

    func updateElectricityWindow(_ electricity: [String: Any]) {
        let available = electricity["available"] as? Bool ?? false
        let stale = electricity["stale"] as? Bool ?? false
        let state = electricity["payment_state"] as? String ?? "idle"
        let amount = (electricity["payment_amount_yuan"] as? NSNumber)?.intValue ?? 100
        let building = (electricity["building"] as? NSNumber)?.intValue ?? 0
        let room = (electricity["room"] as? NSNumber)?.intValue ?? 0
        electricityBalance.stringValue = available ? String(format: "剩余 %.2f 度%@", (electricity["remaining_kwh"] as? NSNumber)?.doubleValue ?? 0, stale ? "（缓存）" : "") : "等待余额更新…"
        let caption = ["idle":"选择金额生成充值码", "creating":"正在生成充值码…", "ready":"待支付", "expired":"充值码已过期，请重新生成", "paid":"支付成功，电量到账中", "failed":"支付未成功，请重试", "error":"充值码获取失败，请重试"][state] ?? "等待数据"
        let forecast = (electricity["estimated_days"] as? NSNumber).map { String(format: "预计 %.1f 天 · 日均 %.2f 度", $0.doubleValue, (electricity["daily_kwh"] as? NSNumber)?.doubleValue ?? 0) } ?? "预计天数统计中（需满24小时）"
        electricityMeta.stringValue = electricityRequestError ?? "\(building) 栋 · \(room) 室\n\(caption)\n\(forecast)"
        let expiry = (electricity["expires_at"] as? NSNumber)?.doubleValue ?? 0
        let remaining = expiry - Date().timeIntervalSince1970
        let showQR = available && !stale && !electricityPosting && electricityRequestError == nil && state == "ready" && remaining > 0
        electricityPlaceholder.isHidden = showQR
        electricityQR.image = showQR ? makeQRCode(electricity["qr_code"] as? String) : nil
        electricityCountdown.stringValue = showQR ? String(format: "%d 元 · %02d:%02d 后过期", amount, Int(remaining) / 60, Int(remaining) % 60) : "选择金额后生成二维码"
        electricityGenerate.isEnabled = available && !stale && !electricityPosting && state != "creating"
        electricityCheck.isEnabled = !electricityPosting
        electricityLater.isEnabled = !electricityPosting
    }

    func makeQRCode(_ value: String?) -> NSImage? {
        guard let value, !value.isEmpty, let filter = CIFilter(name: "CIQRCodeGenerator") else { return nil }
        filter.setValue(Data(value.utf8), forKey: "inputMessage")
        filter.setValue("M", forKey: "inputCorrectionLevel")
        guard let output = filter.outputImage else { return nil }
        let scale = electricityWindow?.backingScaleFactor ?? 2
        let side = Int(220 * scale)
        let factor = max(1, side / (Int(output.extent.width) + 8))
        guard let image = CIContext().createCGImage(output, from: output.extent),
              let context = CGContext(data: nil, width: side, height: side, bitsPerComponent: 8, bytesPerRow: 0,
                                      space: CGColorSpaceCreateDeviceGray(), bitmapInfo: CGImageAlphaInfo.none.rawValue) else { return nil }
        context.setFillColor(CGColor(gray: 1, alpha: 1))
        context.fill(CGRect(x: 0, y: 0, width: side, height: side))
        context.interpolationQuality = .none
        let width = image.width * factor
        context.draw(image, in: CGRect(x: (side - width) / 2, y: (side - width) / 2, width: width, height: width))
        guard let qr = context.makeImage() else { return nil }
        return NSImage(cgImage: qr, size: NSSize(width: 220, height: 220))
    }

    func windowShouldClose(_ sender: NSWindow) -> Bool {
        if sender === electricityWindow && electricityVisible { dismissElectricity() }
        return true
    }

    func startService() {
        guard backend?.isRunning != true else { return }
        let executable = Bundle.main.bundleURL.appendingPathComponent("Contents/MacOS/ReporterBackend")
        let process = Process()
        process.executableURL = executable
        process.arguments = ["--port", String(port)]
        process.standardOutput = FileHandle.nullDevice
        process.standardError = FileHandle.nullDevice
        process.terminationHandler = { [weak self] finished in
            DispatchQueue.main.async {
                guard let self = self, self.backend === finished else { return }
                self.backend = nil
                self.stopping = false
                self.refresh()
            }
        }
        do {
            try process.run()
            backend = process
            launchedAt = Date()
            offline()
        } catch {
            backend = nil
            offline()
            detailLabel.stringValue = "启动失败：\(error.localizedDescription)"
        }
    }

    @objc func toggleService() {
        if let process = backend, process.isRunning {
            stopping = true
            toggle.isEnabled = false
            process.terminate()
            offline()
        } else {
            startService()
        }
    }

    func applicationDidBecomeActive(_ notification: Notification) {
        if loginSwitch != nil { refreshLoginStatus() }
    }

    func refreshLoginStatus() {
        guard #available(macOS 13.0, *) else {
            loginSwitch.isEnabled = false
            loginSettings.isEnabled = false
            loginStatus.stringValue = "此功能需要 macOS 13 或更新版本"
            return
        }
        switch SMAppService.mainApp.status {
        case .enabled:
            loginSwitch.state = .on
            loginStatus.stringValue = "已开启，下次登录在菜单栏后台启动"
        case .requiresApproval:
            loginSwitch.state = .on
            loginStatus.stringValue = "等待系统允许，请前往系统登录项开启"
        case .notRegistered:
            loginSwitch.state = .off
            loginStatus.stringValue = "已关闭，登录后需手动打开程序"
        case .notFound:
            loginSwitch.state = .off
            loginStatus.stringValue = "系统未找到应用，请重新打开完整应用"
        @unknown default:
            loginSwitch.state = .off
            loginStatus.stringValue = "状态未知，请检查系统登录项"
        }
    }

    @objc func toggleLogin() {
        guard #available(macOS 13.0, *) else { return }
        do {
            if loginSwitch.state == .on {
                try SMAppService.mainApp.register()
            } else {
                try SMAppService.mainApp.unregister()
            }
        } catch {
            let alert = NSAlert()
            alert.messageText = "无法更改自启动设置"
            alert.informativeText = error.localizedDescription
            alert.addButton(withTitle: "好")
            alert.beginSheetModal(for: window)
        }
        refreshLoginStatus()
    }

    @objc func openLoginSettings() {
        if #available(macOS 13.0, *) { SMAppService.openSystemSettingsLoginItems() }
    }

    @objc func openDiagnostic() {
        NSWorkspace.shared.open(URL(string: "http://127.0.0.1:\(port)/api/v1/status")!)
    }

    @objc func openLogs() {
        try? FileManager.default.createDirectory(at: dataDirectory, withIntermediateDirectories: true)
        NSWorkspace.shared.open(dataDirectory)
    }

    @objc func aboutAuthor() {
        let alert = NSAlert()
        alert.messageText = "关于作者"
        alert.informativeText = "希娜 Syna · v1.0.0\n作者：黑沐\n\nB 站 UID：386856267\nQQ：3091479711\n\n官方仓库：github.com/heimumumu/Waveshare_ESP32_RLCD\n发布下载：仓库 Releases 页面\n\nCopyright © 2026 黑沐\n原创部分采用 MIT；第三方组件保留各自授权。"
        alert.addButton(withTitle: "关闭")
        alert.addButton(withTitle: "官方仓库")
        alert.addButton(withTitle: "发布下载")
        alert.beginSheetModal(for: window) { response in
            if response == .alertSecondButtonReturn {
                NSWorkspace.shared.open(URL(string: "https://github.com/heimumumu/Waveshare_ESP32_RLCD")!)
            } else if response == .alertThirdButtonReturn {
                NSWorkspace.shared.open(URL(string: "https://github.com/heimumumu/Waveshare_ESP32_RLCD/releases")!)
            }
        }
    }

    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        quitting = true
        timer?.invalidate()
        guard let process = backend, process.isRunning else { return .terminateNow }
        process.terminate()
        DispatchQueue.global().async {
            process.waitUntilExit()
            DispatchQueue.main.async { NSApp.reply(toApplicationShouldTerminate: true) }
        }
        return .terminateLater
    }
}

// Preserve command-line and worker behavior for existing launchers and tests.
if !CommandLine.arguments.contains("--installer-enable-login") && CommandLine.arguments.dropFirst().contains(where: { $0.hasPrefix("--") }) {
    let executable = URL(fileURLWithPath: CommandLine.arguments[0]).deletingLastPathComponent().appendingPathComponent("ReporterBackend").path
    let arguments = [executable] + Array(CommandLine.arguments.dropFirst())
    let pointers = arguments.map { strdup($0) } + [nil]
    execv(executable, pointers)
    exit(1)
}
let application = NSApplication.shared
let delegate = ReporterApp()
application.delegate = delegate
application.run()
