/**
 * Chrono-shift 示例 JS 插件
 */
(function () {
    'use strict';

    var manifest = {
        id: 'com.chronoshift.example',
        name: '示例插件',
        version: '1.0.0',
        type: 'js',
        description: '一个简单的示例插件',
        author: 'Chrono-shift Team',
        ipc_types: [0x70, 0x71]
    };

    var api = {
        /** 插件初始化 */
        onInit: function () {
            console.log('[ExamplePlugin] 初始化完成');
            PluginAPI.storage('com.chronoshift.example', 'initialized', true);
        },

        /** 收到 IPC 消息 */
        onIpcMessage: function (type, data) {
            console.log('[ExamplePlugin] IPC消息 type=0x' + type.toString(16), data);
        },

        /** 插件销毁 */
        onDestroy: function () {
            console.log('[ExamplePlugin] 已卸载');
        }
    };

    PluginAPI.register(manifest, api);
})();
