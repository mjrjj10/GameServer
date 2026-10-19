var outDir = 'D:\\MxM\\ClientEUdump\\';
var installed = false, done = {}, count = 0;
function readPath(obj) {
    try {
        var cap = obj.add(0x20).readU32();
        var p = cap > 7 ? obj.add(0xc).readPointer() : obj.add(0xc);
        return p.readUtf16String();
    } catch (e) { return null; }
}
function install() {
    if (installed) return; installed = true;
    Interceptor.attach(ptr('0x12fdbd9'), {
        onEnter: function (args) { this.obj = this.context.ecx; this.buf = args[0]; this.len = args[1].toInt32(); },
        onLeave: function (ret) {
            var path = readPath(this.obj);
            if (!path || this.len <= 0 || this.len > 64 * 1024 * 1024) return;
            var rel = path.replace(/\//g, '\\');
            var i = rel.toLowerCase().indexOf('\\data\\');
            if (i >= 0) rel = rel.substring(i + 6);
            rel = rel.replace(/^(\.\.\\)+/, '');
            if (!/\.xml$/i.test(rel)) return;
            var key = rel.toLowerCase(), owner = this.obj.toString();
            if (done[key] && done[key] !== owner) return;
            var first = !done[key];
            done[key] = owner;
            try {
                var f = new File(outDir + rel.replace(/\\/g, '__'), first ? 'wb' : 'ab');
                f.write(this.buf.readByteArray(this.len));
                f.close();
                count++;
                if (first) console.log('dump ' + rel + ' len=' + this.len);
            } catch (e) { console.log('err ' + rel + ' ' + e); }
        }
    });
    console.log('hooked');
}
Interceptor.attach(Module.getGlobalExportByName('CreateFileW'), {
    onEnter: function (args) { try { var p = args[0].readUtf16String(); if (p && p.indexOf('.pak') >= 0) install(); } catch (e) {} }
});
install();
console.log("ready");
