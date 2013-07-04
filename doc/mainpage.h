/*!
 * \mainpage QtContacts-Tracker - Tracker backend for QtContacts API
 *
 * \section intro_sec About
 * The plugin is a backend for QtContacts, therefore you probably want to read
 * the Qt Mobility documentation first.
 *
 * \section extensions_sec QtContacts extensions
 * QtContacts-Tracker implements a few extensions to the mobility API:
 * <ul>
 * <li>QctContactMergeRequest - Allows you to merge several contacts together</li>
 * <li>QctUnmergeIMContactsRequest - Allows you to unmerge IM accounts from a contact</li>
 * </ul>
 * Those classes are in the qtcontacts-extensions-tracker library.
 *
 * \section gc_sec Garbage collection
 * There is also an interface to contactsd's garbage collection plugin: QctGarbageCollector
 *
 * \section engineparams_sec Engine parameters
 * Finally, you can pass a variety of parameter when creating the QContactManager, see
 * the QContactTrackerEngineParameters documentation.
 */
